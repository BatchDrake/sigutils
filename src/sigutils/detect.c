/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

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

#include <sigutils/detect.h>

#include <string.h>

#include <assert.h>
#include <sigutils/sampling.h>
#include <sigutils/taps.h>

SU_CONSTRUCTOR(su_peak_detector, unsigned int size, SUFLOAT thres)
{
  SUFLOAT *history;

  assert(self != NULL);
  assert(size > 0);

  SU_ALLOCATE_MANY_CATCH(history, size, SUFLOAT, return SU_FALSE);

  self->size = size;
  self->thr2 = thres * thres;
  self->history = history;
  self->p = 0;
  self->count = 0;
  self->accum = 0.0;
  self->inv_size = 1. / size;

  return SU_TRUE;
}

SU_METHOD(su_peak_detector, int, feed, SUFLOAT x)
{
  SUFLOAT mean;
  SUFLOAT variance = .0;
  SUFLOAT d;
  SUFLOAT x2;
  SUFLOAT threshold;
  unsigned int i;
  int peak = 0;

  /* There are essentially two work regimes here:
   *
   * 1. Filling up the history buffer. We can't tell whether the passed
   *    sample is a peak or not, so we return 0.
   * 2. Overwriting the history buffer. We update the mean to calculate the
   *    STD, compare it against the incoming sample and return
   *    0, 1 or -1 accordingly.
   */

  if (self->count < self->size) {
    /* Populate */
    self->history[self->count++] = x;
  } else {
    mean = self->inv_size * self->accum;

    /* Compute variance */
    for (i = 0; i < self->size; ++i) {
      d = self->history[i] - mean;
      variance += d * d;
    }

    variance *= self->inv_size;

    x2 = x - mean;
    x2 *= x2;
    threshold = self->thr2 * variance;

    if (x2 > threshold) {
      peak = x > mean ? 1 : -1;
    }

    /* Remove last sample from accumulator */
    self->accum -= self->history[self->p];
    self->history[self->p++] = x;

    if (self->p == self->size)
      self->p = 0; /* Rollover */
  }

  self->accum += x;

  return peak;
}

SU_DESTRUCTOR(su_peak_detector)
{
  if (self->history != NULL)
    free(self->history);
}

/*
 * Channel detector use case will work as follows:
 *
 * ---- CHANNEL DISCOVERY ----
 * 1. Initialize with a given damping factor for the FFT.
 * 2. Feed it with samples.
 * 3. After some runs, perform channel detection and register a channel.
 * 4. Optionally, rerun channel detection and adjust channel parameters.
 *
 * ---- CYCLOSTATIONARY ANALYSIS ----
 * 1. Initialize:
 *    1. Same damping factor.
 *    2. Tune to the center frequency of the selected channel
 *    3. Set mode to SU_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY
 * 2. Feed it with samples. Channel detector will compute the FFT of
 *    the current signal times the conjugate of the previous sinal.
 * 3. Apply peak detector in the channel frequency range.
 * 4. First peak should be the baudrate. Accuracy of the estimation can be
 *    improved by increasing the decimation.
 *
 * ---- ORDER ESTIMATION ----
 * 1. Initialize:
 *    1. Same damping factor.
 *    2. Tune to the center frequency of the selected channel
 *    3. Set mode to SU_CHANNEL_DETECTOR_MODE_ORDER ESTIMATION
 * 2. Feed it with samples (this will compute a power).
 * 3. Apply peak detector in the channel frequency range after some runs.
 * 4. At least two peaks should appear. If a third peak appears in the
 *    center frequency, order has been found. Otherwise, estimator should
 *    be reset and start again.
 */

SU_COPY_INSTANCER(su_channel)
{
  su_channel_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_channel_t);

  memcpy(new, self, sizeof(su_channel_t));

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_channel, new);
  return NULL;
}

SU_COLLECTOR(su_channel)
{
  free(self);
}

SUPRIVATE
SU_METHOD(su_channel_detector, void, channel_list_clear)
{
  struct sigutils_channel *chan;

  FOR_EACH_PTR(chan, self, channel)
  SU_DISPOSE(su_channel, chan);

  if (self->channel_list != NULL)
    free(self->channel_list);

  self->channel_count = 0;
  self->channel_list = NULL;
}

SUPRIVATE
SU_METHOD(su_channel_detector, void, channel_collect)
{
  unsigned int i;

  for (i = 0; i < self->channel_count; ++i)
    if (self->channel_list[i] != NULL)
      if (self->channel_list[i]->age++ > 2 * self->channel_list[i]->present) {
        su_channel_destroy(self->channel_list[i]);
        self->channel_list[i] = NULL;
      }
}

SU_GETTER(su_channel_detector, su_channel_t *, lookup_channel, SUFLOAT fc)
{
  su_channel_t *chan;

  FOR_EACH_PTR(chan, self, channel)
  if (fc >= chan->fc - chan->bw * .5 && fc <= chan->fc + chan->bw * .5)
    return chan;

  return NULL;
}

SU_GETTER(su_channel_detector, su_channel_t *, lookup_valid_channel, SUFLOAT fc)
{
  su_channel_t *chan;

  FOR_EACH_PTR(chan, self, channel)
  if (SU_CHANNEL_IS_VALID(chan))
    if (fc >= chan->fc - chan->bw * .5 && fc <= chan->fc + chan->bw * .5)
      return chan;

  return NULL;
}

SUPRIVATE SUBOOL
su_channel_detector_assert_channel(
    su_channel_detector_t *self,
    const struct sigutils_channel *new)
{
  su_channel_t *chan = NULL, *owned = NULL;
  SUFLOAT k = .5;
  SUBOOL ok = SU_FALSE;

  if ((chan = su_channel_detector_lookup_channel(self, new->fc)) == NULL) {
    SU_ALLOCATE(owned, su_channel_t);

    owned->bw = new->bw;
    owned->fc = new->fc;
    owned->f_lo = new->f_lo;
    owned->f_hi = new->f_hi;

    SU_TRYC(PTR_LIST_APPEND_CHECK(self->channel, owned));

    chan = owned;
    owned = NULL;
  } else {
    chan->present++;
    if (chan->age > 20)
      k /= (chan->age - 20);

    /* The older the channel is, the harder it must be to change its params */
    chan->bw += 1. / (chan->age + 1) * (new->bw - chan->bw);
    chan->f_lo += 1. / (chan->age + 1) * (new->f_lo - chan->f_lo);
    chan->f_hi += 1. / (chan->age + 1) * (new->f_hi - chan->f_hi);
    chan->fc += 1. / (chan->age + 1) * (new->fc - chan->fc);
  }

  /* Signal levels are instantaneous values. Cannot average */
  chan->S0 = new->S0;
  chan->N0 = new->N0;
  chan->snr = new->S0 - new->N0;

  ok = SU_TRUE;

done:
  if (owned != NULL)
    SU_DISPOSE(su_channel, owned);

  return ok;
}

SU_COLLECTOR(su_channel_detector)
{
  if (self->fft_plan != NULL)
    SU_FFTW(_destroy_plan)(self->fft_plan);

  if (self->fft_plan_rev != NULL)
    SU_FFTW(_destroy_plan)(self->fft_plan_rev);

  if (self->window != NULL)
    SU_FFTW(_free)(self->window);

  if (self->window_func != NULL)
    SU_FFTW(_free)(self->window_func);

  if (self->fft != NULL)
    SU_FFTW(_free)(self->fft);

  if (self->ifft != NULL)
    SU_FFTW(_free)(self->ifft);

  if (self->_r_alloc != NULL)
    free(self->_r_alloc);

  if (self->spmax != NULL)
    free(self->spmax);

  if (self->spmin != NULL)
    free(self->spmin);

  su_channel_detector_channel_list_clear(self);

  SU_DESTRUCT(su_softtuner, &self->tuner);

  if (self->tuner_buf != NULL)
    free(self->tuner_buf);

  SU_DESTRUCT(su_peak_detector, &self->pd);

  free(self);
}

SU_GETTER(
    su_channel_detector,
    void,
    get_channel_list,
    struct sigutils_channel ***channel_list,
    unsigned int *channel_count)
{
  *channel_list = self->channel_list;
  *channel_count = self->channel_count;
}

SU_METHOD(
    su_channel_detector,
    SUBOOL,
    set_params,
    const struct sigutils_channel_detector_params *params)
{
  SU_TRYCATCH(params->alpha > .0, return SU_FALSE);
  SU_TRYCATCH(params->samp_rate > 0, return SU_FALSE);
  SU_TRYCATCH(params->decimation > 0, return SU_FALSE);

  /*
   * New window_size settings requires re-allocating all FFTW objects. It's
   * better off if we just create a new detector object
   */
  if (params->window_size != self->params.window_size)
    return SU_FALSE;

  /*
   * Different window functions also require different preallocated
   * buffers.
   */
  if (params->window != self->params.window)
    return SU_FALSE;

  /* Changing the detector bandwidth implies recreating the antialias filter */
  if (params->bw != self->params.bw)
    return SU_FALSE;

  /*
   * Changing the sample rate if an antialias filter has been set also
   * requires re-allocation.
   */
  if (params->bw > 0.0 && params->samp_rate != self->params.samp_rate)
    return SU_FALSE;

  /* It's okay to change the parameters now */
  self->params = *params;

  /* Initialize local oscillator */
  if (params->tune)
    su_channel_detector_set_fc(&self->tuner, params->fc);

  return SU_TRUE;
}

SUINLINE
SU_METHOD(su_channel_detector, SUBOOL, init_window_func)
{
  unsigned int i;

  for (i = 0; i < self->params.window_size; ++i)
    self->window_func[i] = 1;

  switch (self->params.window) {
    case SU_CHANNEL_DETECTOR_WINDOW_NONE:
      /* Do nothing. */
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_HAMMING:
      su_taps_apply_hamming_complex(
          self->window_func,
          self->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_HANN:
      su_taps_apply_hann_complex(self->window_func, self->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_FLAT_TOP:
      su_taps_apply_flat_top_complex(
          self->window_func,
          self->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS:
      su_taps_apply_blackmann_harris_complex(
          self->window_func,
          self->params.window_size);
      break;

    default:
      /*
       * This surely will generate thousands of messages, but it should
       * never happen either
       */
      SU_WARNING("Unsupported window function %d\n", self->params.window);
      return SU_FALSE;
  }

  return SU_TRUE;
}

SU_INSTANCER(
    su_channel_detector,
    const struct sigutils_channel_detector_params *params)
{
  su_channel_detector_t *new = NULL;
  struct sigutils_softtuner_params tuner_params =
      sigutils_softtuner_params_INITIALIZER;

  assert(params->alpha > .0);
  assert(params->samp_rate > 0);
  assert(params->window_size > 0);
  assert(params->decimation > 0);

  SU_ALLOCATE_FAIL(new, su_channel_detector_t);

  new->params = *params;

  if ((new->window =
           SU_FFTW(_malloc)(params->window_size * sizeof(SU_FFTW(_complex))))
      == NULL) {
    SU_ERROR("cannot allocate memory for window\n");
    goto fail;
  }

  memset(new->window, 0, params->window_size * sizeof(SU_FFTW(_complex)));

  if ((new->window_func =
           SU_FFTW(_malloc)(params->window_size * sizeof(SU_FFTW(_complex))))
      == NULL) {
    SU_ERROR("cannot allocate memory for window function\n");
    goto fail;
  }

  SU_TRYCATCH(su_channel_detector_init_window_func(new), goto fail);

  if ((new->fft =
           SU_FFTW(_malloc)(params->window_size * sizeof(SU_FFTW(_complex))))
      == NULL) {
    SU_ERROR("cannot allocate memory for FFT\n");
    goto fail;
  }

  memset(new->fft, 0, params->window_size * sizeof(SU_FFTW(_complex)));

  /*
   * Generic result allocation: the same buffer is used differently depending
   * on the detector mode
   */
  SU_ALLOCATE_MANY_FAIL(new->_r_alloc, params->window_size, SUFLOAT);

  /* Direct FFT plan */
  if ((new->fft_plan = su_lib_plan_dft_1d(
           params->window_size,
           new->window,
           new->fft,
           FFTW_FORWARD,
           su_lib_fftw_strategy()))
      == NULL) {
    SU_ERROR("failed to create FFT plan\n");
    goto fail;
  }

  /* Mode-specific allocations */
  switch (params->mode) {
    case SU_CHANNEL_DETECTOR_MODE_SPECTRUM:
    case SU_CHANNEL_DETECTOR_MODE_ORDER_ESTIMATION:
      break;

    case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
      /* Discovery mode requires these max/min levels */
      if ((new->spmax = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
        SU_ERROR("cannot allocate memory for max\n");
        goto fail;
      }

      if ((new->spmin = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
        SU_ERROR("cannot allocate memory for min\n");
        goto fail;
      }
      break;

    case SU_CHANNEL_DETECTOR_MODE_AUTOCORRELATION:
      /* For inverse FFT */
      if ((new->ifft = SU_FFTW(_malloc)(
               params->window_size * sizeof(SU_FFTW(_complex))))
          == NULL) {
        SU_ERROR("cannot allocate memory for IFFT\n");
        goto fail;
      }

      memset(new->ifft, 0, params->window_size * sizeof(SU_FFTW(_complex)));

      if ((new->fft_plan_rev = su_lib_plan_dft_1d(
               params->window_size,
               new->fft,
               new->ifft,
               FFTW_BACKWARD,
               su_lib_fftw_strategy()))
          == NULL) {
        SU_ERROR("failed to create FFT plan\n");
        goto fail;
      }
      break;

    case SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF:
      /* We only need a peak detector here */
      if (!su_peak_detector_init(&new->pd, params->pd_size, params->pd_thres)) {
        SU_ERROR("failed to initialize peak detector\n");
        goto fail;
      }

      break;
  }

  /* Initialize tuner (if enabled) */
  if (params->tune) {
    SU_ALLOCATE_MANY_FAIL(
        new->tuner_buf,
        SU_BLOCK_STREAM_BUFFER_SIZE,
        SUCOMPLEX);

    memset(new->tuner_buf, 0, SU_BLOCK_STREAM_BUFFER_SIZE * sizeof(SUCOMPLEX));

    tuner_params.fc = params->fc;
    tuner_params.bw = params->bw;
    tuner_params.samp_rate = params->samp_rate;
    tuner_params.decimation = params->decimation;

    SU_CONSTRUCT_FAIL(su_softtuner, &new->tuner, &tuner_params);
  }

  /* Calculate the required number of samples to perform detection */
  new->req_samples = 0; /* We can perform detection immediately */

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_channel_detector, new);

  return NULL;
}

SU_GETTER(su_channel_detector, SUSCOUNT, get_req_samples)
{
  return self->req_samples;
}

SUPRIVATE
SU_METHOD(su_channel_detector, SUBOOL, find_channels)
{
  unsigned int i;
  unsigned int N;
  unsigned int fs;
  SUCOMPLEX acc; /* Accumulator for the autocorrelation technique */
  SUFLOAT psd;   /* Power spectral density in this FFT bin */
  SUFLOAT nfreq; /* Normalized frequency of this FFT bin */
  SUFLOAT peak_S0;
  SUFLOAT power;
  SUFLOAT squelch;
  struct sigutils_channel new_channel = sigutils_channel_INITIALIZER;
  SUBOOL c = SU_FALSE; /* Channel flag */

  squelch = self->params.snr * self->N0;

  N = self->params.window_size;
  fs = self->params.samp_rate;

  for (i = 0; i < N; ++i) {
    psd = self->spect[i];
    nfreq = 2 * i / (SUFLOAT)N;

    /* Below threshold */
    if (!c) {
      /* Channel found? */
      if (psd > squelch) {
        c = SU_TRUE;
        acc = psd * SU_C_EXP(I * M_PI * nfreq);
        peak_S0 = psd;
        power = psd;
        new_channel.f_lo = SU_NORM2ABS_FREQ(fs, nfreq);
      }
    } else { /* Above threshold */
      if (psd > squelch) {
        /*
         * We use the autocorrelation technique to estimate the center
         * frequency. It is based in the fact that the lag-one autocorrelation
         * equals to PSD times a phase factor that matches that of the
         * frequency bin. What we actually compute here is the centroid of
         * a cluster of points in the I/Q plane.
         */
        acc += psd * SU_C_EXP(I * M_PI * nfreq);
        power += psd;

        if (psd > peak_S0)
          peak_S0 += self->params.gamma * (psd - peak_S0);
      } else {
        /* End of channel? */
        c = SU_FALSE;

        /* Populate channel information */
        new_channel.f_hi = SU_NORM2ABS_FREQ(fs, nfreq);
        new_channel.S0 = SU_POWER_DB(peak_S0);
        new_channel.N0 = SU_POWER_DB(self->N0);
        new_channel.bw = SU_NORM2ABS_FREQ(fs, 2. * power / (peak_S0 * N));
        new_channel.fc = SU_NORM2ABS_FREQ(fs, SU_ANG2NORM_FREQ(SU_C_ARG(acc)));

        /* Assert it */
        if (!su_channel_detector_assert_channel(self, &new_channel)) {
          SU_ERROR("Failed to register a channel\n");
          return SU_FALSE;
        }
      }
    }
  }

  return SU_TRUE;
}

void
su_channel_params_adjust(struct sigutils_channel_detector_params *params)
{
  SUFLOAT equiv_fs;
  SUFLOAT alpha;

  if (params->decimation < 1)
    params->decimation = 1;

  /*
   * We can link alpha to a ponderation factor used to average the PSD
   * along a given time window. If we asume this time span to be 1 second:
   *
   * With fs =  4096: we have 1 update of the FFT per second.
   * With fs =  8192: we have 2 updates of the FFT per second
   * With fs = 65536: we have 16 updates of the FFT per second
   *
   * Therefore, in the first case, 1 FFT is all the spectral information
   * we have along 1 second, in the second case, 1 FFT is *half* the
   * spectral information we have along 1 second and so on. So the
   * alpha value could be proportional to the inverse of the number of
   * FFT calculations per time window. This number depends on the sample
   * rate, of course:
   *
   *             F
   * alpha = ---------
   *          fs * T
   *
   * Where:
   *   fs := Equivalent sample rate (after decimation)
   *   F  := Number of FFT bins
   *   T  := Time window
   *
   * With this formula, for some degenerate cases, alpha may be 1. We just
   * detect that case and set alpha to 1.
   */

  equiv_fs = (SUFLOAT)params->samp_rate / params->decimation;
  alpha = (SUFLOAT)params->window_size
          / (equiv_fs * SU_CHANNEL_DETECTOR_AVG_TIME_WINDOW);
  params->alpha = MIN(alpha, 1.);
}

void
su_channel_params_adjust_to_channel(
    struct sigutils_channel_detector_params *params,
    const struct sigutils_channel *channel)
{
  struct sigutils_softtuner_params tuner_params =
      sigutils_softtuner_params_INITIALIZER;

  tuner_params.samp_rate = params->samp_rate;

  su_softtuner_params_adjust_to_channel(&tuner_params, channel);

  params->decimation = tuner_params.decimation;
  params->bw = tuner_params.bw;
  params->fc = tuner_params.fc;

  su_channel_params_adjust(params);
}

SUPRIVATE
SU_METHOD(su_channel_detector, SUBOOL, perform_discovery)
{
  unsigned int i;
  unsigned int N;           /* FFT size */
  unsigned int valid;       /* valid FFT bins */
  unsigned int min_pwr_bin; /* bin of the stpectrogram where the min power is */
  SUFLOAT beta;
  SUFLOAT min_pwr; /* minimum power density */
  SUFLOAT psd;     /* current power density */
  SUFLOAT N0;      /* Noise level */

  SUBOOL detector_enabled; /* whether we can detect channels */

  N = self->params.window_size;

  if (self->iters++ == 0) {
    /* First run */
    memcpy(self->spmax, self->spect, N * sizeof(SUFLOAT));
    memcpy(self->spmin, self->spect, N * sizeof(SUFLOAT));

    /* First estimation of the noise floor */
    if (self->N0 == 0) {
      N0 = INFINITY;
      for (i = 0; i < N; ++i)
        if (self->spect[i] < N0)
          N0 = self->spect[i];

      self->N0 = N0;
    }
  } else {
    /* Next runs */

    beta = self->params.beta;

    detector_enabled = self->req_samples == 0;
    N0 = 0;
    valid = 0;
    min_pwr = INFINITY;
    min_pwr_bin = -1;

    for (i = 0; i < N; ++i) {
      psd = self->spect[i];

      /* Update minimum */
      if (psd < self->spmin[i])
        self->spmin[i] = psd;
      else
        self->spmin[i] += beta * (psd - self->spmin[i]);

      /* Update maximum */
      if (psd > self->spmax[i])
        self->spmax[i] = psd;
      else
        self->spmax[i] += beta * (psd - self->spmax[i]);

      if (detector_enabled) {
        /* Use previous N0 estimation to detect outliers */
        if (self->spmin[i] < self->N0 && self->N0 < self->spmax[i]) {
          N0 += psd;
          ++valid;
        }

        /* Update minimum power */
        if (psd < min_pwr) {
          min_pwr_bin = i;
          min_pwr = psd;
        }
      }
    }

    if (detector_enabled) {
      if (valid != 0)
        self->N0 = N0 / valid;
      else if (min_pwr_bin != -1)
        self->N0 = .5 * (self->spmin[min_pwr_bin] + self->spmax[min_pwr_bin]);
    }

    /* Check whether max age has been reached and clear channel list */
    if (self->iters >= self->params.max_age) {
      self->iters = 0;
      su_channel_detector_channel_list_clear(self);
    }

    /* New threshold calculated, find channels */
    if (!su_channel_detector_find_channels(self))
      return SU_FALSE;

    su_channel_detector_channel_collect(self);
  }

  return SU_TRUE;
}

SUPRIVATE
SU_METHOD(su_channel_detector, SUBOOL, find_baudrate_from_acorr)
{
  int i;
  int N;
  SUFLOAT prev, this, next;
  SUFLOAT norm;
  SUFLOAT dtau;
  SUFLOAT tau;

  N = self->params.window_size;
  dtau = (SUFLOAT)self->params.decimation / (SUFLOAT)self->params.samp_rate;

  prev = self->acorr[0];

  /* Find first valley */
  for (i = 1; i < N - 1; ++i) {
    next = self->acorr[i + 1];
    this = self->acorr[i];
    prev = self->acorr[i - 1];

    if (this < next && this < prev)
      break; /* Valley found */
  }

  /* No valley found */
  if (i == N - 1) {
    self->baud = 0;
  } else {
    /* If prev < next, the null is between (prev, this] */
    if (prev < next) {
      norm = 1. / (prev + this);
      tau = norm * dtau * (prev * i + this * (i - 1));
    } else { /* Otherwise, it's between [this, next) */
      norm = 1. / (next + this);
      tau = norm * dtau * (next * i + this * (i + 1));
    }

    self->baud = 1. / tau;
  }

  return SU_TRUE;
}

SUPRIVATE
SU_METHOD(
    su_channel_detector,
    SUBOOL,
    guess_baudrate,
    SUFLOAT equiv_fs,
    int bin,
    SUFLOAT signif)
{
  int N;
  int j;
  int hi, lo;
  SUCOMPLEX acc = 0;
  SUFLOAT floor = 0;

  N = self->params.window_size;

  /* Measure significance w.r.t the surrounding noise floor */
  hi = lo = -1;

  /* Find channel limits */
  for (j = bin + 1; j < N; ++j)
    if (self->spect[j] > self->spect[j - 1]) {
      hi = j;
      break;
    }

  for (j = bin - 1; j >= 0; --j)
    if (self->spect[j] > self->spect[j + 1]) {
      lo = j;
      break;
    }

  if (hi != -1 && lo != -1) {
    floor = .5 * (self->spect[hi] + self->spect[lo]);

    /* Is significance high enough? */
    if (SU_DB(self->spect[bin] / floor) > signif) {
      acc = 0;

      /*
       * Perform an accurate estimation of the baudrate using the
       * autocorrelation technique
       */
      for (j = lo + 1; j < hi; ++j)
        acc += SU_C_EXP(2 * I * M_PI * j / (SUFLOAT)N) * self->spect[j];
      self->baud = SU_NORM2ABS_FREQ(equiv_fs, SU_ANG2NORM_FREQ(SU_C_ARG(acc)));
      return SU_TRUE;
    }
  }

  return SU_FALSE;
}

SUPRIVATE
SU_METHOD(su_channel_detector, SUBOOL, find_baudrate_nonlinear)
{
  int i, N;
  int max_idx;

  SUSCOUNT startbin;
  SUFLOAT dbaud;
  SUFLOAT max;
  SUFLOAT equiv_fs;

  N = self->params.window_size;
  equiv_fs = (SUFLOAT)self->params.samp_rate / (SUFLOAT)self->params.decimation;
  dbaud = equiv_fs / N;

  /*
   * We always reset the baudrate. We prefer to fail here instead of
   * giving a non-accurate estimation.
   */
  self->baud = 0;

  /*
   * We implement two ways to get the baudrate. We look first for the
   * second biggest peak in the spectrum (the first is the DC). If
   * the significante is not big enough w.r.t the surrounding floor,
   * we fall back to the old way.
   */

  /*
   * First step: find where the DC ends */
  for (i = 1; i < N / 2 && (self->spect[i] < self->spect[i - 1]); ++i)
    ;

  /* Second step: look for the second largest peak */
  max_idx = -1;
  max = .0;

  while (i < N / 2) {
    if (self->spect[i] > max) {
      max_idx = i;
      max = self->spect[i];
    }
    ++i;
  }

  /* Peak found. Verify if its significance is big enough */
  if (max_idx != -1)
    if (su_channel_detector_guess_baudrate(
            self,
            equiv_fs,
            max_idx,
            self->params.pd_signif))
      return SU_TRUE;

  /* Previous method failed. Fall back to the old way */
  if (self->params.bw != 0.0) {
    startbin = SU_CEIL(.5 * self->params.bw / dbaud) - self->params.pd_size;
    if (startbin < 0) {
      /*
       * Fail silently here. The current configuration of the
       * channel detector just makes nonlinear detection impossible
       */
      return SU_TRUE;
    }
    i = startbin;
  } else {
    i = 1;
  }

  while (i < N / 2) {
    if (su_peak_detector_feed(&self->pd, SU_DB(self->spect[i])) > 0)
      if (su_channel_detector_guess_baudrate(
              self,
              equiv_fs,
              i,
              self->params.pd_signif))
        break;
    ++i;
  }

  return SU_TRUE;
}

SUINLINE
SU_METHOD(su_channel_detector, void, apply_window)
{
  unsigned int i;

  for (i = self->next_to_window; i < self->ptr; ++i)
    self->window[i] *= self->window_func[i];

  self->next_to_window = self->ptr;
}

SU_METHOD(su_channel_detector, SUBOOL, exec_fft)
{
  unsigned int i;
  SUFLOAT psd;
  SUFLOAT wsizeinv = 1. / self->params.window_size;
  SUFLOAT ac;

  if (self->fft_issued)
    return SU_TRUE;

  self->fft_issued = SU_TRUE;

  switch (self->params.mode) {
    case SU_CHANNEL_DETECTOR_MODE_SPECTRUM:
      /* Spectrum mode only */
      ++self->iters;
      su_channel_detector_apply_window(self);
      SU_FFTW(_execute(self->fft_plan));

      for (i = 0; i < self->params.window_size; ++i)
        self->spect[i] =
            wsizeinv * SU_C_REAL(self->fft[i] * SU_C_CONJ(self->fft[i]));

      return SU_TRUE;

    case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
      /*
       * Channel detection is based on the analysis of the power spectrum
       */
      su_channel_detector_apply_window(self);

      SU_FFTW(_execute(self->fft_plan));

      self->dc += SU_CHANNEL_DETECTOR_DC_ALPHA
                  * (self->fft[0] / self->params.window_size - self->dc);

      /* Update DC component */
      for (i = 0; i < self->params.window_size; ++i) {
        psd = wsizeinv * SU_C_REAL(self->fft[i] * SU_C_CONJ(self->fft[i]));
        self->spect[i] += self->params.alpha * (psd - self->spect[i]);
      }

      return su_channel_detector_perform_discovery(self);

    case SU_CHANNEL_DETECTOR_MODE_AUTOCORRELATION:
      /*
       * Find repetitive patterns in received signal. We find them
       * using the Fast Auto-Correlation Technique, which is relies on
       * two FFTs - O(2nlog(n)) - rather than its definition - O(n^2)
       */

      /* Don't apply *any* window function */
      SU_FFTW(_execute(self->fft_plan));
      for (i = 0; i < self->params.window_size; ++i)
        self->fft[i] *= SU_C_CONJ(self->fft[i]);
      SU_FFTW(_execute(self->fft_plan_rev));

      /* Average result */
      for (i = 0; i < self->params.window_size; ++i) {
        ac = SU_C_REAL(self->ifft[i] * SU_C_CONJ(self->ifft[i]));
        self->acorr[i] += self->params.alpha * (ac - self->acorr[i]);
      }

      /* Update baudrate estimation */
      return su_channel_detector_find_baudrate_from_acorr(self);

    case SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF:
      /*
       * Compute FFT of the square of the absolute value of the derivative
       * of the signal. This will introduce a train of pulses on every
       * non-equal symbol transition.
       */
      su_taps_apply_blackmann_harris_complex(
          self->window,
          self->params.window_size);

      SU_FFTW(_execute(self->fft_plan));

      for (i = 0; i < self->params.window_size; ++i) {
        psd = SU_C_REAL(self->fft[i] * SU_C_CONJ(self->fft[i]));
        psd /= self->params.window_size;
        self->spect[i] += self->params.alpha * (psd - self->spect[i]);
      }

      /* Update baudrate estimation */
      return su_channel_detector_find_baudrate_nonlinear(self);

      break;

    default:
      SU_WARNING("Mode not implemented\n");
      return SU_FALSE;
  }

  return SU_TRUE;
}

SUINLINE
SU_METHOD(su_channel_detector, SUBOOL, feed_internal, SUCOMPLEX x)
{
  SUCOMPLEX diff;

  /* In nonlinear diff mode, we store something else in the window */
  if (self->params.mode == SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF) {
    diff = (x - self->prev) * self->params.samp_rate;
    self->prev = x;
    x = diff * SU_C_CONJ(diff);
  }

  self->window[self->ptr++] = x - self->dc;
  self->fft_issued = SU_FALSE;

  if (self->ptr == self->params.window_size) {
    /* Window is full, perform FFT */
    SU_TRYCATCH(su_channel_detector_exec_fft(self), return SU_FALSE);

    self->ptr = 0;
    self->next_to_window = 0;
  }

  return SU_TRUE;
}

SU_METHOD(
    su_channel_detector,
    SUSCOUNT,
    feed_bulk,
    const SUCOMPLEX *signal,
    SUSCOUNT size)
{
  unsigned int i;
  const SUCOMPLEX *tuned_signal;
  SUSCOUNT tuned_size;
  SUSDIFF result;

  if (self->params.tune) {
    su_softtuner_feed(&self->tuner, signal, size);

    result = su_softtuner_read(
        &self->tuner,
        self->tuner_buf,
        SU_BLOCK_STREAM_BUFFER_SIZE);

    tuned_signal = self->tuner_buf;
    tuned_size = result;
  } else {
    tuned_signal = signal;
    tuned_size = size;
  }

  for (i = 0; i < tuned_size; ++i)
    if (!su_channel_detector_feed_internal(self, tuned_signal[i]))
      break;

  return i;
}

SU_METHOD(su_channel_detector, SUBOOL, feed, SUCOMPLEX x)
{
  return su_channel_detector_feed_bulk(self, &x, 1) >= 0;
}
