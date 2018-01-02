/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <string.h>

#include "detect.h"

#include "sampling.h"
#include "taps.h"
#include "assert.h"

SUBOOL
su_peak_detector_init(su_peak_detector_t *pd, unsigned int size, SUFLOAT thres)
{
  SUFLOAT *history;

  assert(pd != NULL);
  assert(size > 0);

  if ((history = malloc(size * sizeof (SUFLOAT))) == NULL)
    return SU_FALSE;

  pd->size = size;
  pd->thr2 = thres * thres;
  pd->history = history;
  pd->p = 0;
  pd->count = 0;
  pd->accum = 0.0;
  pd->inv_size = 1. / size;

  return SU_TRUE;
}

int
su_peak_detector_feed(su_peak_detector_t *pd, SUFLOAT x)
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

  if (pd->count < pd->size) {
    /* Populate */
    pd->history[pd->count++] = x;
  } else {
    mean = pd->inv_size * pd->accum;

    /* Compute variance */
    for (i = 0; i < pd->size; ++i) {
      d = pd->history[i] - mean;
      variance += d * d;
    }

    variance *= pd->inv_size;

    x2 = x - mean;
    x2 *= x2;
    threshold = pd->thr2 * variance;

    if (x2 > threshold) {
      peak = x > mean ? 1 : -1;
    }


    /* Remove last sample from accumulator */
    pd->accum -= pd->history[pd->p];
    pd->history[pd->p++] = x;

    if (pd->p == pd->size)
      pd->p = 0; /* Rollover */
  }

  pd->accum += x;

  return peak;
}

void
su_peak_detector_finalize(su_peak_detector_t *pd)
{
  if (pd->history != NULL)
    free(pd->history);
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

struct sigutils_channel *
su_channel_dup(const struct sigutils_channel *channel)
{
  struct sigutils_channel *new = NULL;

  if ((new = malloc(sizeof(struct sigutils_channel))) == NULL)
    return NULL;

  memcpy(new, channel, sizeof(struct sigutils_channel));

  return new;
}

void
su_channel_destroy(struct sigutils_channel *channel)
{
  free(channel);
}

SUPRIVATE void
su_channel_detector_channel_list_clear(su_channel_detector_t *detector)
{
  struct sigutils_channel *chan;
  unsigned int i;

  FOR_EACH_PTR(chan, i, detector->channel)
    su_channel_destroy(chan);

  if (detector->channel_list != NULL)
    free(detector->channel_list);

  detector->channel_count = 0;
  detector->channel_list  = NULL;
}

SUPRIVATE void
su_channel_detector_channel_collect(su_channel_detector_t *detector)
{
  unsigned int i;

  for (i = 0; i < detector->channel_count; ++i)
    if (detector->channel_list[i] != NULL)
      if (detector->channel_list[i]->age++
          > 2 * detector->channel_list[i]->present) {
        su_channel_destroy(detector->channel_list[i]);
        detector->channel_list[i] = NULL;
      }
}

struct sigutils_channel *
su_channel_detector_lookup_channel(
    const su_channel_detector_t *detector,
    SUFLOAT fc)
{
  struct sigutils_channel *chan;
  unsigned int i;

  FOR_EACH_PTR(chan, i, detector->channel)
    if (fc >= chan->fc - chan->bw * .5 &&
        fc <= chan->fc + chan->bw * .5)
      return chan;

  return NULL;
}

struct sigutils_channel *
su_channel_detector_lookup_valid_channel(
    const su_channel_detector_t *detector,
    SUFLOAT fc)
{
  struct sigutils_channel *chan;
  unsigned int i;

  FOR_EACH_PTR(chan, i, detector->channel)
    if (SU_CHANNEL_IS_VALID(chan))
      if (fc >= chan->fc - chan->bw * .5 &&
          fc <= chan->fc + chan->bw * .5)
        return chan;

  return NULL;
}

SUPRIVATE SUBOOL
su_channel_detector_assert_channel(
    su_channel_detector_t *detector,
    const struct sigutils_channel *new)
{
  struct sigutils_channel *chan = NULL;
  SUFLOAT k = .5;

  if ((chan = su_channel_detector_lookup_channel(detector, new->fc)) == NULL) {
    if ((chan = malloc(sizeof (struct sigutils_channel))) == NULL)
      return SU_FALSE;

    chan->bw      = new->bw;
    chan->fc      = new->fc;
    chan->f_lo    = new->f_lo;
    chan->f_hi    = new->f_hi;
    chan->age     = 0;
    chan->present = 0;

    if (PTR_LIST_APPEND_CHECK(detector->channel, chan) == -1) {
      su_channel_destroy(chan);
      return SU_FALSE;
    }
  } else {
    chan->present++;
    if (chan->age > 20)
      k /=  (chan->age - 20);

    /* The older the channel is, the harder it must be to change its params */
    chan->bw   += 1. / (chan->age + 1) * (new->bw   - chan->bw);
    chan->f_lo += 1. / (chan->age + 1) * (new->f_lo - chan->f_lo);
    chan->f_hi += 1. / (chan->age + 1) * (new->f_hi - chan->f_hi);
    chan->fc   += 1. / (chan->age + 1) * (new->fc   - chan->fc);
  }

  /* Signal levels are instantaneous values. Cannot average */
  chan->S0        = new->S0;
  chan->N0        = new->N0;
  chan->snr       = new->S0 - new->N0;

  return SU_TRUE;
}

void
su_channel_detector_destroy(su_channel_detector_t *detector)
{
  if (detector->fft_plan != NULL)
    SU_FFTW(_destroy_plan)(detector->fft_plan);

  if (detector->fft_plan_rev != NULL)
    SU_FFTW(_destroy_plan)(detector->fft_plan_rev);

  if (detector->window != NULL)
    fftw_free(detector->window);

  if (detector->window_func != NULL)
    fftw_free(detector->window_func);

  if (detector->fft != NULL)
    fftw_free(detector->fft);

  if (detector->ifft != NULL)
    fftw_free(detector->ifft);

  if (detector->_r_alloc != NULL)
    free(detector->_r_alloc);

  if (detector->spmax != NULL)
    free(detector->spmax);

  if (detector->spmin != NULL)
    free(detector->spmin);

  su_channel_detector_channel_list_clear(detector);

  su_softtuner_finalize(&detector->tuner);

  if (detector->tuner_buf != NULL)
    free(detector->tuner_buf);

  su_peak_detector_finalize(&detector->pd);

  free(detector);
}

void
su_channel_detector_get_channel_list(
    const su_channel_detector_t *detector,
    struct sigutils_channel ***channel_list,
    unsigned int *channel_count)
{
  *channel_list = detector->channel_list;
  *channel_count = detector->channel_count;
}

SUBOOL
su_channel_detector_set_params(
    su_channel_detector_t *detector,
    const struct sigutils_channel_detector_params *params)
{
  SU_TRYCATCH(params->alpha > .0, return SU_FALSE);
  SU_TRYCATCH(params->samp_rate > 0, return SU_FALSE);
  SU_TRYCATCH(params->decimation > 0, return SU_FALSE);

  /*
   * New window_size settings requires re-allocating all FFTW objects. It's
   * better if we just create a new detector object
   */
  if (params->window_size != detector->params.window_size)
    return SU_FALSE;

  /* Changing the detector bandwidth implies recreating the antialias filter */
  if (params->bw != detector->params.bw)
    return SU_FALSE;

  /*
   * Changing the sample rate if an antialias filter has been set also
   * requires re-allocation.
   */
  if (params->bw > 0.0 && params->samp_rate != detector->params.samp_rate)
    return SU_FALSE;

  /* It's okay to change the parameters now */
  detector->params = *params;

  /* Initialize local oscillator */
  if (params->tune)
    su_channel_detector_set_fc(&detector->tuner, params->fc);

  return SU_TRUE;
}

SUINLINE SUBOOL
su_channel_detector_init_window_func(su_channel_detector_t *detector)
{
  unsigned int i;

  for (i = 0; i < detector->params.window_size; ++i)
    detector->window_func[i] = 1;

  switch (detector->params.window) {
    case SU_CHANNEL_DETECTOR_WINDOW_NONE:
      /* Do nothing. */
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_HAMMING:
      su_taps_apply_hamming_complex(
          detector->window_func,
          detector->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_HANN:
      su_taps_apply_hann_complex(
          detector->window_func,
          detector->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_FLAT_TOP:
      su_taps_apply_flat_top_complex(
          detector->window_func,
          detector->params.window_size);
      break;

    case SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS:
      su_taps_apply_blackmann_harris_complex(
          detector->window_func,
          detector->params.window_size);
      break;

    default:
      /*
       * This surely will generate thousands of messages, but it should
       * never happen either
       */
      SU_WARNING("Unsupported window function %d\n", detector->params.window);
      return SU_FALSE;
  }

  return SU_TRUE;
}

su_channel_detector_t *
su_channel_detector_new(const struct sigutils_channel_detector_params *params)
{
  su_channel_detector_t *new = NULL;
  struct sigutils_softtuner_params tuner_params
    = sigutils_softtuner_params_INITIALIZER;

  assert(params->alpha > .0);
  assert(params->samp_rate > 0);
  assert(params->window_size > 0);
  assert(params->decimation > 0);

  if ((new = calloc(1, sizeof (su_channel_detector_t))) == NULL)
    goto fail;

  new->params = *params;

  if ((new->window
      = fftw_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for window\n");
    goto fail;
  }

  if ((new->window_func
      = fftw_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for window function\n");
    goto fail;
  }

  SU_TRYCATCH(su_channel_detector_init_window_func(new), goto fail);

  if ((new->fft
      = fftw_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for FFT\n");
    goto fail;
  }

  /*
   * Generic result allocation: the same buffer is used differently depending
   * on the detector mode
   */
  if ((new->_r_alloc
      = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
    SU_ERROR("cannot allocate memory for averaged FFT\n");
    goto fail;
  }

  /* Direct FFT plan */
  if ((new->fft_plan = SU_FFTW(_plan_dft_1d)(
      params->window_size,
      new->window,
      new->fft,
      FFTW_FORWARD,
      FFTW_ESTIMATE)) == NULL) {
    SU_ERROR("failed to create FFT plan\n");
    goto fail;
  }

  /* Mode-specific allocations */
  switch (params->mode) {
    case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
      /* Discovery mode requires these max/min levels */
      if ((new->spmax
          = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
        SU_ERROR("cannot allocate memory for max\n");
        goto fail;
      }

      if ((new->spmin
          = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
        SU_ERROR("cannot allocate memory for min\n");
        goto fail;
      }
      break;

    case SU_CHANNEL_DETECTOR_MODE_AUTOCORRELATION:
      /* For inverse FFT */
      if ((new->ifft
          = fftw_malloc(
              params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
        SU_ERROR("cannot allocate memory for IFFT\n");
        goto fail;
      }

      if ((new->fft_plan_rev = SU_FFTW(_plan_dft_1d)(
          params->window_size,
          new->fft,
          new->ifft,
          FFTW_BACKWARD,
          FFTW_ESTIMATE)) == NULL) {
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
    SU_TRYCATCH(
        new->tuner_buf = malloc(
            SU_BLOCK_STREAM_BUFFER_SIZE * sizeof (SUCOMPLEX)),
        goto fail);

    tuner_params.fc = params->fc;
    tuner_params.bw = params->bw;
    tuner_params.samp_rate = params->samp_rate;
    tuner_params.decimation = params->decimation;

    SU_TRYCATCH(su_softtuner_init(&new->tuner, &tuner_params), goto fail);
  }

  /* Calculate the required number of samples to perform detection */
  new->req_samples = 0; /* We can perform detection immediately */

  return new;

fail:
  if (new != NULL)
    su_channel_detector_destroy(new);

  return NULL;
}

SUSCOUNT
su_channel_detector_get_req_samples(const su_channel_detector_t *detector)
{
  return detector->req_samples;
}

SUPRIVATE SUBOOL
su_channel_detector_find_channels(
    su_channel_detector_t *detector) {
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
  SUBOOL  c = SU_FALSE;  /* Channel flag */
  SUBOOL ok = SU_FALSE;

  squelch = detector->params.snr * detector->N0;

  N = detector->params.window_size;
  fs = detector->params.samp_rate;

  for (i = 0; i < N; ++i) {
    psd = detector->spect[i];
    nfreq = 2 * i / (SUFLOAT) N;

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
          peak_S0 += detector->params.gamma * (psd - peak_S0);
      } else {
        /* End of channel? */
        c = SU_FALSE;

        /* Populate channel information */
        new_channel.f_hi = SU_NORM2ABS_FREQ(fs, nfreq);
        new_channel.S0   = SU_POWER_DB(peak_S0);
        new_channel.N0   = SU_POWER_DB(detector->N0);
        new_channel.bw   = SU_NORM2ABS_FREQ(fs, 2. * power / (peak_S0 * N));
        new_channel.fc   = SU_NORM2ABS_FREQ(
            fs,
            SU_ANG2NORM_FREQ(SU_C_ARG(acc)));

        /* Assert it */
        if (!su_channel_detector_assert_channel(detector, &new_channel)) {
          SU_ERROR("Failed to register a channel\n");
          return SU_FALSE;
        }
      }
    }
  }

  ok = SU_TRUE;

done:
  return ok;
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

  equiv_fs = (SUFLOAT) params->samp_rate / params->decimation;
  alpha = (SUFLOAT) params->window_size /
      (equiv_fs * SU_CHANNEL_DETECTOR_AVG_TIME_WINDOW);
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

SUPRIVATE SUBOOL
su_channel_perform_discovery(su_channel_detector_t *detector)
{
  unsigned int i;
  unsigned int N; /* FFT size */
  unsigned int valid; /* valid FFT bins */
  unsigned int min_pwr_bin; /* bin of the stpectrogram where the min power is */
  SUFLOAT alpha;
  SUFLOAT beta;
  SUFLOAT gamma;
  SUFLOAT min_pwr; /* minimum power density */
  SUFLOAT psd; /* current power density */
  SUFLOAT N0; /* Noise level */

  SUBOOL  detector_enabled; /* whether we can detect channels */

  N = detector->params.window_size;

  if (detector->iters++ == 0) {
    /* First run */
    memcpy(detector->spmax, detector->spect, N * sizeof(SUFLOAT));
    memcpy(detector->spmin, detector->spect, N * sizeof(SUFLOAT));

    /* First estimation of the noise floor */
    N0 = INFINITY;
    for (i = 0; i < N; ++i)
      if (detector->spect[i] < N0)
        N0 = detector->spect[i];

    detector->N0 = N0;
  } else {
    /* Next runs */

    alpha = detector->params.alpha;
    beta  = detector->params.beta;
    gamma = detector->params.gamma;

    detector_enabled = detector->req_samples == 0;
    N0 = 0;
    valid = 0;
    min_pwr = INFINITY;
    min_pwr_bin = -1;

    for (i = 0; i < N; ++i) {
      psd = detector->spect[i];

      /* Update minimum */
      if (psd < detector->spmin[i])
        detector->spmin[i] = psd;
      else
        detector->spmin[i] += beta * (psd - detector->spmin[i]);

      /* Update maximum */
      if (psd > detector->spmax[i])
        detector->spmax[i] = psd;
      else
        detector->spmax[i] += beta * (psd - detector->spmax[i]);

      if (detector_enabled) {
        /* Use previous N0 estimation to detect outliers */
        if (detector->spmin[i] < detector->N0
            || detector->N0 < detector->spmax[i]) {
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
      if (valid == 0)
        detector->N0 = N0 / valid;
      else
        detector->N0 = .5
          * (detector->spmin[min_pwr_bin] + detector->spmax[min_pwr_bin]);
    }

    /* Check whether max age has been reached and clear channel list */
    if (detector->iters >= detector->params.max_age) {
      detector->iters = 0;
      su_channel_detector_channel_list_clear(detector);
    }

    /* New threshold calculated, find channels */
    if (!su_channel_detector_find_channels(detector))
      return SU_FALSE;

    su_channel_detector_channel_collect(detector);
  }

  return SU_TRUE;
}

SUPRIVATE SUBOOL
su_channel_detect_baudrate_from_acorr(su_channel_detector_t *detector)
{
  int i;
  int N;
  SUFLOAT prev, this, next;
  SUFLOAT norm;
  SUFLOAT dtau;
  SUFLOAT tau;

  N = detector->params.window_size;
  dtau = (SUFLOAT) detector->params.decimation
       / (SUFLOAT) detector->params.samp_rate;

  prev = detector->acorr[0];

  /* Find first valley */
  for (i = 1; i < N - 1; ++i) {
    next = detector->acorr[i + 1];
    this = detector->acorr[i];
    prev = detector->acorr[i - 1];

    if (this < next && this < prev)
      break; /* Valley found */
  }

  /* No valley found */
  if (i == N - 1) {
    detector->baud = 0;
  } else {
    /* If prev < next, the null is between (prev, this] */
    if (prev < next) {
      norm = 1. / (prev + this);
      tau = norm * dtau * (prev * i + this * (i - 1));
    } else { /* Otherwise, it's between [this, next) */
      norm = 1. / (next + this);
      tau = norm * dtau * (next * i + this * (i + 1));
    }

    detector->baud = 1. / tau;
  }

  return SU_TRUE;
}

SUPRIVATE SUBOOL
su_channel_detector_guess_baudrate(
    su_channel_detector_t *detector,
    SUFLOAT equiv_fs,
    int bin,
    SUFLOAT signif)
{
  int N;
  int j;
  int hi, lo;
  SUCOMPLEX acc = 0;
  SUFLOAT floor = 0;

  N = detector->params.window_size;

  /* Measure significance w.r.t the surrounding noise floor */
  hi = lo = -1;

  /* Find channel limits */
  for (j = bin + 1; j < N; ++j)
    if (detector->spect[j] > detector->spect[j - 1]) {
      hi = j;
      break;
    }

  for (j = bin - 1; j >= 0; --j)
    if (detector->spect[j] > detector->spect[j + 1]) {
      lo = j;
      break;
    }

  if (hi != -1 && lo != -1) {
    floor = .5 * (detector->spect[hi] + detector->spect[lo]);

    /* Is significance high enough? */
    if (SU_DB(detector->spect[bin] / floor) > signif) {
      acc = 0;

      /*
       * Perform an accurate estimation of the baudrate using the
       * autocorrelation technique
       */
      for (j = lo + 1; j < hi; ++j)
        acc += SU_C_EXP(2 * I * M_PI * j / (SUFLOAT) N) * detector->spect[j];
      detector->baud =
          SU_NORM2ABS_FREQ(equiv_fs, SU_ANG2NORM_FREQ(SU_C_ARG(acc)));
      return SU_TRUE;
    }
  }

  return SU_FALSE;
}

SUPRIVATE SUBOOL
su_channel_detect_baudrate_from_nonlinear_diff(su_channel_detector_t *detector)
{
  int i, N;
  int max_idx;

  SUSCOUNT startbin;
  SUFLOAT dbaud;
  SUFLOAT max;
  SUFLOAT equiv_fs;

  N = detector->params.window_size;
  equiv_fs =
      (SUFLOAT) detector->params.samp_rate
      / (SUFLOAT) detector->params.decimation;
  dbaud = equiv_fs / N;

  /*
   * We always reset the baudrate. We prefer to fail here instead of
   * giving a non-accurate estimation.
   */
  detector->baud = 0;

  /*
   * We implement two ways to get the baudrate. We look first for the
   * second biggest peak in the spectrum (the first is the DC). If
   * the significante is not big enough w.r.t the surrounding floor,
   * we fall back to the old way.
   */

  /*
   * First step: find where the DC ends */
  for (i = 1; i < N / 2 && (detector->spect[i] < detector->spect[i - 1]); ++i);

  /* Second step: look for the second largest peak */
  max_idx = -1;
  max = .0;

  while (i < N / 2) {
    if (detector->spect[i] > max) {
      max_idx = i;
      max = detector->spect[i];
    }
    ++i;
  }

  /* Peak found. Verify if its significance is big enough */
  if (max_idx != -1)
    if (su_channel_detector_guess_baudrate(
        detector,
        equiv_fs,
        max_idx,
        detector->params.pd_signif))
      return SU_TRUE;

  /* Previous method failed. Fall back to the old way */
  if (detector->params.bw != 0.0) {
    startbin =
        SU_CEIL(.5 * detector->params.bw / dbaud) - detector->params.pd_size;
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
    if (su_peak_detector_feed(&detector->pd, SU_DB(detector->spect[i])) > 0)
      if (su_channel_detector_guess_baudrate(
          detector,
          equiv_fs,
          i,
          detector->params.pd_signif))
        break;
    ++i;
  }

  return SU_TRUE;
}

SUINLINE void
su_channel_detector_apply_window(su_channel_detector_t *detector)
{
  unsigned int i;

  for (i = 0; i < detector->params.window_size; ++i)
    detector->window[i] *= detector->window_func[i];
}

SUINLINE SUBOOL
su_channel_detector_feed_internal(su_channel_detector_t *detector, SUCOMPLEX x)
{
  unsigned int i;
  SUFLOAT psd;
  SUCOMPLEX diff;
  SUFLOAT ac;

  /* In nonlinear diff mode, we store something else in the window */
  if (detector->params.mode == SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF) {
     diff = (x - detector->prev) * detector->params.samp_rate;
     detector->prev = x;
     x = diff * SU_C_CONJ(diff);
  }

  detector->window[detector->ptr++] = x;

  if (detector->ptr == detector->params.window_size) {
    /* Window is full, perform FFT */
    detector->ptr = 0;

    /* ^^^^^^^^^^^^^^^^^^ end of common part ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
    switch (detector->params.mode) {
      case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
        /*
         * Channel detection is based on the analysis of the power spectrum
         */
        su_channel_detector_apply_window(detector);

        SU_FFTW(_execute(detector->fft_plan));

        for (i = 0; i < detector->params.window_size; ++i) {
          psd = SU_C_REAL(detector->fft[i] * SU_C_CONJ(detector->fft[i]));
          psd /= detector->params.window_size;
          detector->spect[i] += detector->params.alpha * (psd - detector->spect[i]);
        }

        return su_channel_perform_discovery(detector);

      case SU_CHANNEL_DETECTOR_MODE_AUTOCORRELATION:
        /*
         * Find repetitive patterns in received signal. We find them
         * using the Fast Auto-Correlation Technique, which is relies on
         * two FFTs - O(2nlog(n)) - rather than its definition - O(n^2)
         */

        /* Don't apply *any* window function */
        SU_FFTW(_execute(detector->fft_plan));
        for (i = 0; i < detector->params.window_size; ++i)
          detector->fft[i] *= SU_C_CONJ(detector->fft[i]);
        SU_FFTW(_execute(detector->fft_plan_rev));

        /* Average result */
        for (i = 0; i < detector->params.window_size; ++i) {
          ac = SU_C_REAL(detector->ifft[i] * SU_C_CONJ(detector->ifft[i]));
          detector->acorr[i] +=
              detector->params.alpha * (ac - detector->acorr[i]);
        }

        /* Update baudrate estimation */
        return su_channel_detect_baudrate_from_acorr(detector);

      case SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF:
        /*
         * Compute FFT of the square of the absolute value of the derivative
         * of the signal. This will introduce a train of pulses on every
         * non-equal symbol transition.
         */
        su_taps_apply_blackmann_harris_complex(
            detector->window,
            detector->params.window_size);

        SU_FFTW(_execute(detector->fft_plan));

        for (i = 0; i < detector->params.window_size; ++i) {
          psd = SU_C_REAL(detector->fft[i] * SU_C_CONJ(detector->fft[i]));
          psd /= detector->params.window_size;
          detector->spect[i] += detector->params.alpha * (psd - detector->spect[i]);
        }

        /* Update baudrate estimation */
        return su_channel_detect_baudrate_from_nonlinear_diff(detector);

        break;

      default:

        SU_WARNING("Mode not implemented\n");
        return SU_FALSE;
    }
  }

  return SU_TRUE;
}

SUSCOUNT
su_channel_detector_feed_bulk(
    su_channel_detector_t *detector,
    const SUCOMPLEX *signal,
    SUSCOUNT size)
{
  unsigned int i;
  const SUCOMPLEX *tuned_signal;
  SUSCOUNT tuned_size;
  SUSDIFF result;

  if (detector->params.tune) {
    su_softtuner_feed(&detector->tuner, signal, size);

    result = su_softtuner_read(
        &detector->tuner,
        detector->tuner_buf,
        SU_BLOCK_STREAM_BUFFER_SIZE);

    tuned_signal = detector->tuner_buf;
    tuned_size   = result;
  } else {
    tuned_signal = signal;
    tuned_size   = size;
  }

  for (i = 0; i < tuned_size; ++i)
    if (!su_channel_detector_feed_internal(detector, tuned_signal[i]))
      break;

  return i;
}

SUBOOL
su_channel_detector_feed(su_channel_detector_t *detector, SUCOMPLEX x)
{
  return su_channel_detector_feed_bulk(detector, &x, 1) >= 0;
}
