/*

  Copyright (C) 2021 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#define SU_LOG_DOMAIN "smoothpsd"

#include <sigutils/sigutils.h>
#include <sigutils/smoothpsd.h>
#include <sigutils/taps.h>
#include <string.h>

#if defined(_SU_SINGLE_PRECISION) && HAVE_VOLK
#  define SU_USE_VOLK
#  include <volk/volk.h>
#endif

#define _SWAP(a, b) \
  tmp = a;          \
  a = b;            \
  b = tmp;

SU_INSTANCER(
    su_smoothpsd,
    const struct sigutils_smoothpsd_params *params,
    SUBOOL (*psd_func)(void *userdata, const SUFLOAT *psd, unsigned int size),
    void *userdata)
{
  su_smoothpsd_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_smoothpsd_t);

  SU_TRYZ_FAIL(pthread_mutex_init(&new->mutex, NULL));

  new->mutex_init = SU_TRUE;

  new->psd_func = psd_func;
  new->userdata = userdata;

  new->nominal_rate = params->samp_rate;

  SU_TRY_FAIL(su_smoothpsd_set_params(new, params));

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_smoothpsd, new);

  return NULL;
}

SUPRIVATE
SU_METHOD(su_smoothpsd, SUBOOL, exec_fft)
{
  SUFLOAT wsizeinv = 1. / (self->params.fft_size * self->nominal_rate);

  /* Execute FFT */
  SU_FFTW(_execute(self->fft_plan));

  /* Keep real coefficients only */
#ifdef SU_USE_VOLK
  // self->fft (and its alias self->realfft) are SIMD aligned by fftwf_malloc
  // thus, they should also meet volk's alignment needs
  volk_32fc_x2_multiply_conjugate_32fc(self->fft, self->fft, self->fft, self->params.fft_size);
  volk_32fc_deinterleave_real_32f(self->realfft, self->fft, self->params.fft_size);
  volk_32f_s32f_multiply_32f(self->realfft, self->realfft, wsizeinv, self->params.fft_size);
#else
    unsigned int i;

  for (i = 0; i < self->params.fft_size; ++i)
    self->realfft[i] =
        wsizeinv * SU_C_REAL(self->fft[i] * SU_C_CONJ(self->fft[i]));
#endif

  SU_TRYCATCH(
      (self->psd_func)(self->userdata, self->realfft, self->params.fft_size),
      return SU_FALSE);

  ++self->iters;

  return SU_TRUE;
}

SU_METHOD(su_smoothpsd, SUBOOL, feed, const SUCOMPLEX *data, SUSCOUNT size)
{
  unsigned int chunk;
  SUBOOL mutex_acquired = SU_FALSE;
  SUBOOL ok = SU_FALSE;

  SU_TRYZ(pthread_mutex_lock(&self->mutex));

  mutex_acquired = SU_TRUE;

  if (self->max_p > 0) {
    if (self->max_p >= self->params.fft_size) {
      /* Non-overlapped mode. We copy directly to the FFT buffer */
      while (size > 0) {
        chunk = SU_MIN(size, self->params.fft_size - self->p);

        if (chunk > 0) {
          /* Filling the FFT buffer */
          memcpy(self->fft + self->p, data, chunk * sizeof(SUCOMPLEX));
          self->p += chunk;
        } else {
          /* FFT buffer full, now we just skip samples */
          chunk = SU_MIN(size, self->max_p - self->fft_p);
        }

        size -= chunk;
        data += chunk;
        self->fft_p += chunk;

        /* Time to trigger FFT! */
        if (self->fft_p >= self->max_p) {
          self->fft_p = 0;
          self->p = 0;

          /* Apply window function */
#ifdef SU_USE_VOLK
          volk_32fc_x2_multiply_32fc(self->fft, self->fft, self->window_func,
                  self->params.fft_size);
#else
          unsigned int i;
          for (i = 0; i < self->params.fft_size; ++i)
            self->fft[i] *= self->window_func[i];
#endif

          SU_TRY(su_smoothpsd_exec_fft(self));
        }
      }

    } else {
      /* Overlapped mode. This is a bit trickier. */
      while (size > 0) {
        /* We can copy this much */
        chunk =
            SU_MIN(self->max_p - self->fft_p, self->params.fft_size - self->p);

        /* But maybe we were not fed those many samples */
        if (size < chunk)
          chunk = size;

        /*
         * TODO: Do not perform a copy every time. It should be enough to
         * copy samples only when we are about to perform a FFT.
         */
        memcpy(self->buffer + self->p, data, chunk * sizeof(SUCOMPLEX));

        size -= chunk;
        data += chunk;

        self->p += chunk;
        self->fft_p += chunk;

        if (self->p >= self->params.fft_size)
          self->p = 0;

        /* Time to trigger FFT! */
        if (self->fft_p >= self->max_p) {
          unsigned int copy_cnt = self->params.fft_size - self->p;
          self->fft_p = 0;

          /* put ring buffer time domain IQ data in sequential order */
          memcpy(self->fft, self->buffer + self->p, copy_cnt * sizeof(SUCOMPLEX));
          memcpy(self->fft + copy_cnt, self->buffer, self->p * sizeof(SUCOMPLEX));

          /* Apply window function */
#ifdef SU_USE_VOLK
          volk_32fc_x2_multiply_32fc(self->fft, self->fft, self->window_func,
                  self->params.fft_size);
#else
          unsigned int i;
          for (i = 0; i < self->params.fft_size; ++i)
            self->fft[i] *= self->window_func[i];
#endif

          SU_TRY(su_smoothpsd_exec_fft(self));
        }
      }
    }
  }

  ok = SU_TRUE;

done:
  if (mutex_acquired)
    (void)pthread_mutex_unlock(&self->mutex);

  return ok;
}

SU_METHOD(
    su_smoothpsd,
    SUBOOL,
    set_params,
    const struct sigutils_smoothpsd_params *params)
{
  unsigned int i;
  void *tmp = NULL;
  SUBOOL mutex_acquired = SU_FALSE;

  SU_FFTW(_complex) *window_func = NULL;
  SU_FFTW(_complex) *buffer = NULL;
  SU_FFTW(_complex) *fftbuf = NULL;
  SU_FFTW(_plan) fft_plan = NULL;

  SUBOOL refresh_window_func = params->window != self->params.window;
  SUBOOL ok = SU_FALSE;

  /*
   * We do not acquire the mutex here immediately. This is because FFTW_MEASURE
   * makes the initialization of the fft particularly slow and we can detach
   * it from the modification of the current object.
   */
  if (params->fft_size != self->params.fft_size) {
    if ((window_func =
             SU_FFTW(_malloc)(params->fft_size * sizeof(SU_FFTW(_complex))))
        == NULL) {
      SU_ERROR("cannot allocate memory for window\n");
      goto done;
    }

    if ((buffer =
             SU_FFTW(_malloc)(params->fft_size * sizeof(SU_FFTW(_complex))))
        == NULL) {
      SU_ERROR("cannot allocate memory for circular buffer\n");
      goto done;
    }

    memset(buffer, 0, params->fft_size * sizeof(SU_FFTW(_complex)));

    if ((fftbuf =
             SU_FFTW(_malloc)(params->fft_size * sizeof(SU_FFTW(_complex))))
        == NULL) {
      SU_ERROR("cannot allocate memory for FFT buffer\n");
      goto done;
    }

    memset(fftbuf, 0, params->fft_size * sizeof(SU_FFTW(_complex)));

    /* Direct FFT plan */
    if ((fft_plan = su_lib_plan_dft_1d(
             params->fft_size,
             fftbuf,
             fftbuf,
             FFTW_FORWARD,
             su_lib_fftw_strategy()))
        == NULL) {
      SU_ERROR("failed to create FFT plan\n");
      goto done;
    }

    SU_TRYZ(pthread_mutex_lock(&self->mutex));
    mutex_acquired = SU_TRUE;

    _SWAP(window_func, self->window_func);
    _SWAP(buffer, self->buffer);
    _SWAP(fftbuf, self->fft);
    _SWAP(fft_plan, self->fft_plan);

    self->p = 0;

    refresh_window_func = SU_TRUE;
  }

  if (!mutex_acquired) {
    SU_TRYZ(pthread_mutex_lock(&self->mutex));
    mutex_acquired = SU_TRUE;
  }

  self->params = *params;

  if (refresh_window_func) {
    for (i = 0; i < self->params.fft_size; ++i)
      self->window_func[i] = 1;

    switch (self->params.window) {
      case SU_CHANNEL_DETECTOR_WINDOW_NONE:
        /* Do nothing. */
        break;

      case SU_CHANNEL_DETECTOR_WINDOW_HAMMING:
        su_taps_apply_hamming_complex(self->window_func, self->params.fft_size);
        break;

      case SU_CHANNEL_DETECTOR_WINDOW_HANN:
        su_taps_apply_hann_complex(self->window_func, self->params.fft_size);
        break;

      case SU_CHANNEL_DETECTOR_WINDOW_FLAT_TOP:
        su_taps_apply_flat_top_complex(
            self->window_func,
            self->params.fft_size);
        break;

      case SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS:
        su_taps_apply_blackmann_harris_complex(
            self->window_func,
            self->params.fft_size);
        break;

      default:
        /*
         * This surely will generate thousands of messages, but it should
         * never happen either
         */
        SU_WARNING("Unsupported window function %d\n", self->params.window);
        goto done;
    }
  }

  /* We use the sample rate as timebase for all calculations */
  /*
   * refresh_rate is in Hz. This means that we want a FFT update at every
   * 1 / refresh_rate seconds. This, in samples, is samp_rate / refresh_rate
   */

  if (self->params.refresh_rate > 0)
    self->max_p = SU_ROUND(self->params.samp_rate / self->params.refresh_rate);
  else
    self->max_p = 0;

  self->fft_p = 0;

  ok = SU_TRUE;

done:
  if (mutex_acquired)
    (void)pthread_mutex_unlock(&self->mutex);

  if (fft_plan != NULL)
    SU_FFTW(_destroy_plan)(fft_plan);

  if (window_func != NULL)
    SU_FFTW(_free)(window_func);

  if (buffer != NULL)
    SU_FFTW(_free)(buffer);

  if (fftbuf != NULL)
    SU_FFTW(_free)(fftbuf);

  return ok;
}

SU_COLLECTOR(su_smoothpsd)
{
  if (self->mutex_init)
    pthread_mutex_destroy(&self->mutex);

  if (self->fft_plan != NULL)
    SU_FFTW(_destroy_plan)(self->fft_plan);

  if (self->window_func != NULL)
    SU_FFTW(_free)(self->window_func);

  if (self->buffer != NULL)
    SU_FFTW(_free)(self->buffer);

  if (self->fft != NULL)
    SU_FFTW(_free)(self->fft);

  free(self);
}
