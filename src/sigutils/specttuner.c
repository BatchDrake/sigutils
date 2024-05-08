/*

  Copyright (C) 2018 Gonzalo José Carracedo Carballal

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

#define SU_LOG_DOMAIN "specttuner"

#include <sigutils/specttuner.h>

#include <stdlib.h>
#include <string.h>

#include <sigutils/sigutils.h>
#include <sigutils/sampling.h>
#include <sigutils/taps.h>

#ifdef SU_USE_VOLK
#  define calloc su_volk_calloc
#  define malloc su_volk_malloc
#  define free   volk_free
SUINLINE void *
su_volk_malloc(size_t size)
{
  return volk_malloc(size, volk_get_alignment());
}

SUINLINE void *
su_volk_calloc(size_t nmemb, size_t size)
{
  void *result = su_volk_malloc(nmemb * size);

  if (result != NULL)
    memset(result, 0, nmemb * size);

  return result;
}
#endif /* SU_USE_VOLK */

SUPRIVATE
SU_COLLECTOR(su_specttuner_channel)
{
  if (self->plan[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_destroy_plan)(self->plan[SU_SPECTTUNER_STATE_EVEN]);

  if (self->plan[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_destroy_plan)(self->plan[SU_SPECTTUNER_STATE_ODD]);

  if (self->ifft[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_free)(self->ifft[SU_SPECTTUNER_STATE_EVEN]);

  if (self->ifft[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_free)(self->ifft[SU_SPECTTUNER_STATE_ODD]);

  if (self->fft != NULL)
    SU_FFTW(_free)(self->fft);

  if (self->window != NULL)
    SU_FFTW(_free)(self->window);

  if (self->forward != NULL)
    SU_FFTW(_destroy_plan)(self->forward);

  if (self->backward != NULL)
    SU_FFTW(_destroy_plan)(self->backward);

  if (self->h != NULL)
    SU_FFTW(_free)(self->h);

  free(self);
}

SUPRIVATE
SU_METHOD_CONST(
    su_specttuner,
    void,
    update_channel_filter,
    su_specttuner_channel_t *channel)
{
  SUCOMPLEX tmp;
  unsigned int window_size = self->params.window_size;
  unsigned int window_half = window_size / 2;
  unsigned int i;

  /* First step: Setup ideal filter response */
  memset(channel->h, 0, sizeof(SUCOMPLEX) * window_size);

  for (i = 0; i < channel->halfw; ++i) {
    channel->h[i] = 1;
    channel->h[window_size - i - 1] = 1;
  }

  /* Second step: switch to time domain */
  SU_FFTW(_execute)(channel->backward);

  /* Third step: recenter coefficients to apply window function */
  for (i = 0; i < window_half; ++i) {
    tmp = channel->h[i];
    channel->h[i] = channel->k * channel->h[window_half + i];
    channel->h[window_half + i] = channel->k * tmp;
  }

  /* Fourth step: apply Window function */
  su_taps_apply_blackmann_harris_complex(channel->h, window_size);

  /* Fifth step: recenter back */
  for (i = 0; i < window_half; ++i) {
    tmp = channel->h[i];
    channel->h[i] = channel->h[window_half + i];
    channel->h[window_half + i] = tmp;
  }

  /* Sixth step: move back to frequency domain */
  SU_FFTW(_execute)(channel->forward);
}

SUPRIVATE
SU_METHOD_CONST(
    su_specttuner,
    SUBOOL,
    init_filter_response,
    su_specttuner_channel_t *channel)
{
  SUBOOL ok = SU_FALSE;
  unsigned int window_size = self->params.window_size;

  /* Backward plan */
  SU_TRY(
      channel->forward = su_lib_plan_dft_1d(
          window_size,
          channel->h,
          channel->h,
          FFTW_FORWARD,
          su_lib_fftw_strategy()));

  /* Forward plan */
  SU_TRY(
      channel->backward = su_lib_plan_dft_1d(
          window_size,
          channel->h,
          channel->h,
          FFTW_BACKWARD,
          su_lib_fftw_strategy()));

  su_specttuner_update_channel_filter(self, channel);

  ok = SU_TRUE;
done:

  return ok;
}

SUPRIVATE
SU_METHOD_CONST(
    su_specttuner,
    void,
    refresh_channel_center,
    su_specttuner_channel_t *channel)
{
  unsigned int window_size = self->params.window_size;
  SUFLOAT rbw = 2 * PI / window_size;
  SUFLOAT off = 0, ef;

  ef = su_specttuner_channel_get_effective_freq(channel);
  channel->center = 2 * SU_FLOOR(.5 * (ef + 1 * rbw) / (2 * PI) * window_size);

  if (channel->center < 0) {
    channel->center = 0;
  }
  if (channel->center >= window_size)
    channel->center = window_size - 2;

  if (channel->params.precise) {
    off = (channel->center) * rbw - ef;
    off *= channel->decimation;
    su_ncqo_set_angfreq(&channel->lo, off);
  }

  if (channel->params.on_freq_changed != NULL) {
    channel->params.on_freq_changed(
      channel,
      channel->params.privdata,
      channel->old_f0,
      channel->params.f0);
  }
}

SU_METHOD_CONST(
    su_specttuner,
    void,
    set_channel_freq,
    su_specttuner_channel_t *channel,
    SUFLOAT f0)
{
  channel->old_f0 = channel->params.f0;
  channel->params.f0 = f0;
  channel->pending_freq = SU_TRUE;
}

SU_METHOD_CONST(
    su_specttuner,
    void,
    set_channel_delta_f,
    su_specttuner_channel_t *channel,
    SUFLOAT delta_f)
{
  channel->params.delta_f = delta_f;
  channel->pending_freq = SU_TRUE;
}

SU_METHOD_CONST(
    su_specttuner,
    SUBOOL,
    set_channel_bandwidth,
    su_specttuner_channel_t *channel,
    SUFLOAT bw)
{
  SUFLOAT k;
  unsigned int width;

  unsigned int window_size = self->params.window_size;

  if (bw > 2 * PI)
    bw = 2 * PI;

  /*
   * Don't respect guard bands. They are just a hint for the user
   * to give her some margin.
   */

  k = 1. / (2 * PI / bw);
  width = SU_CEIL(k * window_size);

  /* Accounts for rounding errors */
  if (width > window_size)
    width = window_size;

  SU_TRYCATCH(width <= channel->size, return SU_FALSE);
  SU_TRYCATCH(width > 1, return SU_FALSE);

  channel->width = width;
  channel->halfw = channel->width >> 1;

  su_specttuner_update_channel_filter(self, channel);

  return SU_TRUE;
}

SUPRIVATE
SU_INSTANCER(
    su_specttuner_channel,
    const su_specttuner_t *owner,
    const struct sigutils_specttuner_channel_params *params)
{
  su_specttuner_channel_t *new = NULL;
  unsigned int window_size = owner->params.window_size;
  SUFLOAT rbw = 2 * PI / window_size;
  unsigned int n = 1;
  unsigned int i;
  unsigned int min_size;
  SUFLOAT actual_bw;
  SUFLOAT off;
  SUFLOAT corrbw;
  SUFLOAT effective_freq;
  SUBOOL full_spectrum = SU_FALSE;

  if (params->guard < 1) {
    SU_ERROR(
        "Guard bandwidth is smaller than channel bandwidth (guard = %g < 1)\n",
        params->guard);
    goto fail;
  }

  if (params->bw <= 0) {
    SU_ERROR("Cannot open a zero-bandwidth channel\n");
    goto fail;
  }

  effective_freq = params->f0 + params->delta_f;

  if (effective_freq < 0 || effective_freq >= 2 * PI) {
    SU_ERROR("Channel center frequency is outside the spectrum\n");
    goto fail;
  }

  corrbw = params->bw;

  if (corrbw > 2 * PI)
    corrbw = 2 * PI;

  SU_ALLOCATE_FAIL(new, su_specttuner_channel_t);

  actual_bw = corrbw * params->guard;

  if (actual_bw >= 2 * PI) {
    actual_bw = 2 * PI;
    full_spectrum = SU_TRUE;
  }

  new->params = *params;
  new->index = -1;

  if (!full_spectrum) {
    /* Tentative configuration */
    new->k = 1. / (2 * PI / actual_bw);
    /*
     * XXX: THERE IS SOMETHING HERE I COULD NOT FULLY UNDERSTAND
     *
     * For some reason, if we do not pick an even FFT bin for frequency
     * centering, AM components will show up at the decimator output. This
     * is probably related to some symmetry condition not being met
     *
     * TODO: Look into this ASAP
     */

    new->center =
        2 * SU_FLOOR(.5 * (effective_freq + 1 * rbw) / (2 * PI) * window_size);
    min_size = SU_CEIL(new->k *window_size);

    /* Find the nearest power of 2 than can hold all these samples */
    while (n < min_size)
      n <<= 1;

    new->size = n;

    new->width = SU_CEIL(min_size / params->guard);
    new->halfw = new->width >> 1;
  } else {
    new->k = 1. / (2 * PI / params->bw);
    new->center =
        2 * SU_FLOOR(.5 * (effective_freq + 1 * rbw) / (2 * PI) * window_size);
    new->size = window_size;
    new->width = SU_CEIL(new->k *window_size);
    if (new->width > window_size)
      new->width = window_size;
    new->halfw = new->width >> 1;
  }

  /* Adjust configuration to new size */
  new->decimation = window_size / new->size;
  new->k = 1. / (new->decimation *new->size);

  /*
   * High precision mode: initialize local oscillator to compensate
   * for rounding errors introduced by bin index calculation
   */
  if (params->precise) {
    off = new->center *(2 * PI) / (SUFLOAT)window_size - effective_freq;
    off *= new->decimation;
    su_ncqo_init(&new->lo, SU_ANG2NORM_FREQ(off));
  }

  new->halfsz = new->size >> 1;
  new->offset = new->size >> 2;

  new->gain = 1.;

  SU_TRY_FAIL(new->width > 0);

  /*
   * Window function. We leverage fftw(f)_malloc aligned addresses
   * to attempt some kind of cache efficiency here.
   */
  SU_TRY_FAIL(new->window = SU_FFTW(_malloc)(new->size * sizeof(SUFLOAT)));

  SU_TRY_FAIL(
      new->h = SU_FFTW(_malloc)(window_size * sizeof(SU_FFTW(_complex))));

  SU_TRY_FAIL(su_specttuner_init_filter_response(owner, new));

  if (owner->params.early_windowing) {
    for (i = 0; i < new->size; ++i)
      new->window[i] = 1.;
  } else {
    /*
     * Squared cosine window. Seems odd, right? Well, it turns out that
     * since we are storing the square of half a cycle, when we add the
     * odd and even halves we are actually adding up something weighted
     * by two squared cosine halves DELAYED one quarter of a cycle.
     *
     * This is equivalent to adding up something weighted by a squared SINE
     * and a squared COSINE. And it can be proven that cos^2 + sin^2 = 1.
     *
     * In the end, we are favouring central IFFT values before those in
     * the borders. This is something that we generally want.
     *
     * PS: We use SU_SIN instead of SU_COS because we are assuming that
     * the 0 is at new->size/2.
     */
    for (i = 0; i < new->size; ++i) {
      new->window[i] = SU_SIN(PI * (SUFLOAT)i / new->size);
      new->window[i] *= new->window[i];
    }
  }
  
  /* FFT initialization */
  SU_TRY_FAIL(
      new->ifft[SU_SPECTTUNER_STATE_EVEN] =
          SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))));

  SU_TRY_FAIL(
      new->ifft[SU_SPECTTUNER_STATE_ODD] =
          SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))));

  SU_TRY_FAIL(
      new->fft = SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))));

  memset(new->fft, 0, new->size * sizeof(SU_FFTW(_complex)));

  memset(
      new->ifft[SU_SPECTTUNER_STATE_EVEN],
      0,
      new->size * sizeof(SU_FFTW(_complex)));

  memset(
      new->ifft[SU_SPECTTUNER_STATE_ODD],
      0,
      new->size * sizeof(SU_FFTW(_complex)));

  SU_TRY_FAIL(
      new->plan[SU_SPECTTUNER_STATE_EVEN] = su_lib_plan_dft_1d(
          new->size,
          new->fft,
          new->ifft[SU_SPECTTUNER_STATE_EVEN],
          FFTW_BACKWARD,
          su_lib_fftw_strategy()));

  SU_TRY_FAIL(
      new->plan[SU_SPECTTUNER_STATE_ODD] = su_lib_plan_dft_1d(
          new->size,
          new->fft,
          new->ifft[SU_SPECTTUNER_STATE_ODD],
          FFTW_BACKWARD,
          su_lib_fftw_strategy()));

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_specttuner_channel, new);

  return NULL;
}

SU_METHOD(
    su_specttuner,
    su_specttuner_plan_t *,
    make_plan,
    SUCOMPLEX *in)
{
  su_specttuner_plan_t *new = NULL;
  SUBOOL ok = SU_FALSE;
  SUSCOUNT offset;

  offset = in == self->fft ? 0 : self->params.window_size / 2;

  SU_MAKE(new, su_specttuner_plan, in, self->fft, self->params.window_size, offset);

  SU_TRYC(PTR_LIST_APPEND_CHECK(self->plan, new));

  ok = SU_TRUE;

done:
  if (!ok) {
    if (new != NULL) {
      su_specttuner_plan_destroy(new);
      new = NULL;
    }
  }

  return new;
}

SU_COLLECTOR(su_specttuner)
{
  unsigned int i;

  for (i = 0; i < self->channel_count; ++i)
    if (self->channel_list[i] != NULL)
      (void)su_specttuner_close_channel(self, self->channel_list[i]);

  if (self->channel_list != NULL)
    free(self->channel_list);

  for (i = 0; i < self->plan_count; ++i)
    if (self->plan_list[i] != NULL)
      su_specttuner_plan_destroy(self->plan_list[i]);

  if (self->plan_list != NULL)
    free(self->plan_list);
  
  if (self->fft != NULL)
    SU_FFTW(_free)(self->fft);

  if (self->wfunc != NULL)
    free(self->wfunc);

  if (self->buffer != NULL && self->params.buffer != self->buffer)
    SU_FFTW(_free)(self->buffer);

  free(self);
}

SU_INSTANCER(
  su_specttuner_plan,
  SUCOMPLEX *in,
  SUCOMPLEX *out,
  SUSCOUNT size,
  SUSCOUNT offset)
{
  su_specttuner_plan_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_specttuner_plan_t);

  SU_TRY_FAIL(
      new->plans[SU_SPECTTUNER_STATE_EVEN] = su_lib_plan_dft_1d(
          size,
          in,
          out,
          FFTW_FORWARD,
          su_lib_fftw_strategy()));

  /* Odd plan stars at window_size / 2 */
  SU_TRY_FAIL(
      new->plans[SU_SPECTTUNER_STATE_ODD] = su_lib_plan_dft_1d(
          size,
          in + offset,
          out,
          FFTW_FORWARD,
          su_lib_fftw_strategy()));
  
  return new;

fail:
  if (new != NULL)
    su_specttuner_plan_destroy(new);

  return NULL;
}

SU_COLLECTOR(su_specttuner_plan)
{
  if (self->plans[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_destroy_plan)(self->plans[SU_SPECTTUNER_STATE_EVEN]);

  if (self->plans[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_destroy_plan)(self->plans[SU_SPECTTUNER_STATE_ODD]);
}

SU_INSTANCER(su_specttuner, const struct sigutils_specttuner_params *params)
{
  su_specttuner_t *new = NULL;
  unsigned int full_size;
  unsigned int i;

  SU_TRYCATCH((params->window_size & 1) == 0, goto fail);

  SU_ALLOCATE_FAIL(new, su_specttuner_t);

  new->params    = *params;
  new->half_size = params->window_size >> 1;
  full_size      = 3 * new->half_size;

  /* If we use this with custom buffers, we never do early windowing */
  if (new->params.buffer == NULL)
    new->params.early_windowing = SU_FALSE;
  
  /* Early windowing enabled */
  if (new->params.early_windowing) {
    SU_TRY_FAIL(new->wfunc = malloc(params->window_size * sizeof(SUFLOAT)));

    for (i = 0; i < params->window_size; ++i) {
      new->wfunc[i]  = SU_SIN(PI * (SUFLOAT) i / params->window_size);
      new->wfunc[i] *= new->wfunc[i];
    }
  }
  
  if (new->params.buffer == NULL) {
    /* Custom buffer: 3/2 the FFT size */
    SU_TRY_FAIL(
        new->buffer =
            SU_FFTW(_malloc(full_size * sizeof(SU_FFTW(_complex)))));
    memset(new->buffer, 0, full_size * sizeof(SU_FFTW(_complex)));
  } else {
    new->buffer = new->params.buffer;
  }

  /* FFT is the size provided by params */
  SU_TRY_FAIL(
      new->fft =
          SU_FFTW(_malloc(params->window_size * sizeof(SU_FFTW(_complex)))));

  memset(new->fft, 0, params->window_size * sizeof(SU_FFTW(_complex)));

  if (new->params.early_windowing) {
    SU_TRY_FAIL(
      new->default_plan = su_specttuner_make_plan(new, new->fft));
  } else {
    SU_TRY_FAIL(
      new->default_plan = su_specttuner_make_plan(new, new->buffer));
  }

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_specttuner, new);

  return NULL;
}

SU_METHOD(su_specttuner, void, run_fft, su_specttuner_plan_t *plan)
{
  /* Early windowing, copy windowed input */
  if (self->params.early_windowing) {
    if (self->state == SU_SPECTTUNER_STATE_EVEN) {
#ifdef SU_USE_VOLK
      volk_32fc_32f_multiply_32fc(
        self->fft,
        self->buffer,
        self->wfunc,
        self->params.window_size);
#else
      unsigned int i;
      for (i = 0; i < self->params.window_size; ++i)
        self->fft[i] = self->buffer[i] * self->wfunc[i];
#endif /* SU_USE_VOLK */
    } else {
#ifdef SU_USE_VOLK
      volk_32fc_32f_multiply_32fc(
        self->fft,
        self->buffer + self->half_size,
        self->wfunc,
        self->params.window_size);
#else
      unsigned int i;
      for (i = 0; i < self->params.window_size; ++i)
        self->fft[i] = self->buffer[i + self->half_size] * self->wfunc[i];
#endif /* SU_USE_VOLK */
    }
  }

  /* Compute FFT */
  su_specttuner_plan_execute(plan, self->state);
}

SUINLINE SUSCOUNT
__su_specttuner_feed_bulk(
    su_specttuner_t *self,
    const SUCOMPLEX *__restrict buf,
    SUSCOUNT size)
{
  SUSDIFF halfsz;
  SUSDIFF p;
  
  if (size + self->p > self->params.window_size)
    size = self->params.window_size - self->p;

  switch (self->state) {
    case SU_SPECTTUNER_STATE_EVEN:
      /* Just copy at the beginning */
      memcpy(self->buffer + self->p, buf, size * sizeof(SUCOMPLEX));
      break;

    case SU_SPECTTUNER_STATE_ODD:
      /* Copy to the second third */
      memcpy(
          self->buffer + self->p + self->half_size,
          buf,
          size * sizeof(SUCOMPLEX));

      /* 
       * Did this copy populate the last third? In that case, the
       * contents of the last third must be mirrored to the
       * first third.
       */
      if (self->p + size > self->half_size) {
        halfsz = self->p + size - self->half_size;
        p = self->p > self->half_size ? self->p : self->half_size;

        /* Don't take into account data already written */
        halfsz -= p - self->half_size;

        /* Copy to the first third */
        if (halfsz > 0)
          memcpy(
              self->buffer + p - self->half_size,
              self->buffer + p + self->half_size,
              halfsz * sizeof(SUCOMPLEX));
      }
  }

  self->p += size;

  if (self->p == self->params.window_size) {
    self->p = self->half_size;

    su_specttuner_run_fft(self, self->default_plan);

    /* Toggle state */
    self->state = !self->state;
    self->ready = SU_TRUE;
  }

  return size;
}

SUINLINE SUBOOL
__su_specttuner_feed_channel(
    const su_specttuner_t *self,
    su_specttuner_channel_t *channel)
{
  int p;
  int len;
  int window_size = self->params.window_size;
  unsigned int i;
  SUCOMPLEX phase, phold;
  SUFLOAT alpha, beta;
  SUBOOL changing_freqs = SU_FALSE;
  int a_sign, b_sign;
  SUCOMPLEX *prev, *curr;

  /*
   * This is how the phase continuity trick works: as soon as a new
   * center frequency is signaled, we make a copy of the current
   * LO state. Next, during overlapping, the previous buffer is
   * adjusted by the old LO, while the most recent buffer is
   * adjusted by the new LO. Also, phase continuity between bins is
   * ensured, as all bins refer to frequencies multiple of 2pi.
   */

  b_sign = 1 - (channel->center & 2);

  if (self->state && channel->pending_freq) {
    channel->pending_freq = SU_FALSE;

    su_ncqo_copy(&channel->old_lo, &channel->lo);

    su_specttuner_refresh_channel_center(self, channel);

    changing_freqs = SU_TRUE;
  }

  p = channel->center;
  a_sign = 1 - (channel->center & 2);

  /***************************** Upper sideband ******************************/
  len = channel->halfw;
  if (p + len > window_size) /* Test for rollover */
    len = window_size - p;

  /* Copy to the end */
  memcpy(channel->fft, self->fft + p, len * sizeof(SUCOMPLEX));

  /* Copy remaining part */
  if (len < channel->halfw)
    memcpy(
        channel->fft + len,
        self->fft,
        (channel->halfw - len) * sizeof(SUCOMPLEX));

  /***************************** Lower sideband ******************************/
  len = channel->halfw;
  if (p < len) /* Roll over */
    len = p;   /* Can copy up to p bytes */

  /* Copy higher frequencies */
  memcpy(
      channel->fft + channel->size - len,
      self->fft + p - len,
      len * sizeof(SUCOMPLEX));

  /* Copy remaining part */
  if (len < channel->halfw)
    memcpy(
        channel->fft + channel->size - channel->halfw,
        self->fft + window_size - (channel->halfw - len),
        (channel->halfw - len) * sizeof(SUCOMPLEX));

  if (channel->params.domain == SU_SPECTTUNER_CHANNEL_TIME_DOMAIN) {
    /*********************** Apply filter and scaling ************************/
#ifdef SU_SPECTTUNER_SQUARE_FILTER
    for (i = 0; i < channel->halfw; ++i)
      channel->fft[i] *= channel->k;

    for (i = channel->size - channel->halfw; i < channel->size; ++i)
      channel->fft[i] *= channel->k;
#else
    for (i = 0; i < channel->halfsz; ++i) {
      channel->fft[i] *= channel->k * channel->h[i];
      channel->fft[channel->size - i - 1] *=
          channel->k * channel->h[window_size - i - 1];
    }
#endif
  } else {
    /* Channel is defined in the frequency domain. This means that we
       do not need to perform get back to the time domain (hence we can
       go ahead and skip one IFFT completely) */
    
    if (self->state == SU_SPECTTUNER_STATE_EVEN) {
      memcpy(
        channel->fft + channel->halfw,
        channel->fft + channel->size - channel->halfw,
        channel->halfw * sizeof(SUCOMPLEX));
      
      for (i = 0; i < channel->width; ++i)
        channel->fft[i] *= channel->k;
      
      curr = channel->fft;

      return (channel->params.on_data)(
        channel,
        channel->params.privdata,
        curr,
        channel->width);
    } else {
      return SU_TRUE;
    }
  }

  /************************* Back to time domain******************************/
  SU_FFTW(_execute)(channel->plan[self->state]);

  curr = channel->ifft[self->state];
  prev = channel->ifft[!self->state] + channel->halfsz;

  /* Glue buffers */
  if (channel->params.precise) {
    if (changing_freqs) {
      /* Do this only when switching frequencies */
      if (self->params.early_windowing) {
        /* Windowing already applied, no need to apply it here */
        for (i = 0; i < channel->halfsz; ++i) {
          phold = su_ncqo_read(&channel->old_lo);
          phase = su_ncqo_read(&channel->lo);
          curr[i] =
              channel->gain * (phase * curr[i] + phold * prev[i]);
        }
      } else {
        /* Late windowing, need to apply it here */
        for (i = 0; i < channel->halfsz; ++i) {
          alpha = channel->window[i];
          beta = channel->window[i + channel->halfsz];

          phold = su_ncqo_read(&channel->old_lo);
          phase = su_ncqo_read(&channel->lo);
          curr[i] =
              channel->gain * (phase * alpha * curr[i] + phold * beta * prev[i]);
        }
      }
    } else {
      if (self->params.early_windowing) {
        /* Early windowing, speed things up */
        for (i = 0; i < channel->halfsz; ++i) {
          phase = su_ncqo_read(&channel->lo);
          curr[i] = channel->gain * phase * (curr[i] + prev[i]);
        }
      } else {
        /* Late windowing, need to apply it here */
        for (i = 0; i < channel->halfsz; ++i) {
          alpha = channel->window[i];                  /* Positive slope */
          beta = channel->window[i + channel->halfsz]; /* Negative slope */

          phase = su_ncqo_read(&channel->lo);
          curr[i] = channel->gain * phase * (alpha * curr[i] + beta * prev[i]);
        }
      }
    }
  } else {
    if (self->params.early_windowing) {
      /* Early windowing */
      for (i = 0; i < channel->halfsz; ++i)
        curr[i] = channel->gain * (a_sign * curr[i] + b_sign * prev[i]);
    } else {
      /* Late windowing */
      for (i = 0; i < channel->halfsz; ++i) {
        alpha = a_sign * channel->window[i];                  /* Positive slope */
        beta = b_sign * channel->window[i + channel->halfsz]; /* Negative slope */

        curr[i] = channel->gain * (alpha * curr[i] + beta * prev[i]);
      }
    }
  }

  /************************** Call user callback *****************************/
  return (channel->params.on_data)(
      channel,
      channel->params.privdata,
      curr,
      channel->halfsz);
}

SU_METHOD(su_specttuner, SUBOOL, feed_all_channels)
{
  unsigned int i;
  SUBOOL ok = SU_TRUE;

  if (su_specttuner_new_data(self)) {
    for (i = 0; i < self->channel_count; ++i)
      if (self->channel_list[i] != NULL)
        ok = __su_specttuner_feed_channel(self, self->channel_list[i]) && ok;
    su_specttuner_ack_data(self);
  }

  return ok;
}

/* 
 * This assumes that the buffers are appropriately populated, with the
 * mirrored thirds in each place.
 */

SU_METHOD(su_specttuner, SUBOOL, trigger, su_specttuner_plan_t *plan)
{
  unsigned int i;
  SUBOOL ok = SU_TRUE;

  self->p = self->half_size;
  su_specttuner_run_fft(self, plan);
  self->state = !self->state;
  self->ready = SU_TRUE;

  for (i = 0; i < self->channel_count; ++i)
    if (self->channel_list[i] != NULL)
      ok = __su_specttuner_feed_channel(self, self->channel_list[i]) && ok;

  return ok;
}

SU_METHOD(
    su_specttuner,
    SUSDIFF,
    feed_bulk_single,
    const SUCOMPLEX *__restrict buf,
    SUSCOUNT size)
{
  SUSDIFF got;
  SUSCOUNT ok = SU_TRUE;
  unsigned int i;

  if (self->ready)
    return 0;

  got = __su_specttuner_feed_bulk(self, buf, size);

  /* Buffer full, feed channels */
  if (self->ready)
    for (i = 0; i < self->channel_count; ++i)
      if (self->channel_list[i] != NULL)
        ok = __su_specttuner_feed_channel(self, self->channel_list[i]) && ok;

  return ok ? got : -1;
}

SU_METHOD(
    su_specttuner,
    SUBOOL,
    feed_bulk,
    const SUCOMPLEX *__restrict buf,
    SUSCOUNT size)
{
  SUSDIFF got;
  SUBOOL ok = SU_TRUE;

  while (size > 0) {
    got = su_specttuner_feed_bulk_single(self, buf, size);

    if (su_specttuner_new_data(self))
      su_specttuner_ack_data(self);

    if (got == -1)
      ok = SU_FALSE;

    buf += got;
    size -= got;
  }

  return ok;
}

SU_METHOD(
    su_specttuner,
    su_specttuner_channel_t *,
    open_channel,
    const struct sigutils_specttuner_channel_params *params)
{
  su_specttuner_channel_t *new = NULL;
  int index;

  SU_MAKE_FAIL(new, su_specttuner_channel, self, params);

  SU_TRYC_FAIL(index = PTR_LIST_APPEND_CHECK(self->channel, new));

  new->index = index;

  ++self->count;

  return new;

fail:
  if (new != NULL)
    su_specttuner_channel_destroy(new);

  return NULL;
}

SU_METHOD(
    su_specttuner,
    SUBOOL,
    close_channel,
    su_specttuner_channel_t *channel)
{
  SU_TRYCATCH(channel->index >= 0, return SU_FALSE);

  SU_TRYCATCH(channel->index < self->channel_count, return SU_FALSE);

  SU_TRYCATCH(self->channel_list[channel->index] == channel, return SU_FALSE);

  self->channel_list[channel->index] = NULL;

  su_specttuner_channel_destroy(channel);

  --self->count;

  return SU_TRUE;
}
