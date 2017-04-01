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

  if (detector->window != NULL)
    fftw_free(detector->window);

  if (detector->fft != NULL)
    fftw_free(detector->fft);

  if (detector->spect != NULL)
    free(detector->spect);

  if (detector->spmax != NULL)
    free(detector->spmax);

  if (detector->spmin != NULL)
    free(detector->spmin);

  su_channel_detector_channel_list_clear(detector);

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

su_channel_detector_t *
su_channel_detector_new(const struct sigutils_channel_detector_params *params)
{
  su_channel_detector_t *new = NULL;

  assert(params->alpha > .0);
  assert(params->samp_rate > 0);
  assert(params->window_size > 0);
  assert(params->decimation > 0);

  if (params->mode != SU_CHANNEL_DETECTOR_MODE_DISCOVERY) {
    SU_ERROR("unsupported mode\n");
    goto fail;
  }

  if ((new = calloc(1, sizeof (su_channel_detector_t))) == NULL)
    goto fail;

  new->params = *params;

  if ((new->window
      = fftw_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for window\n");
    goto fail;
  }

  if ((new->fft
      = fftw_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for FFT\n");
    goto fail;
  }

  if ((new->spect
      = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
    SU_ERROR("cannot allocate memory for averaged FFT\n");
    goto fail;
  }

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

  if ((new->fft_plan = SU_FFTW(_plan_dft_1d)(
      params->window_size,
      new->window,
      new->fft,
      FFTW_FORWARD,
      FFTW_ESTIMATE)) == NULL) {
    SU_ERROR("failed to create FFT plan\n");
    goto fail;
  }

  su_ncqo_init(
      &new->lo,
      SU_ABS2NORM_FREQ(params->samp_rate, params->fc * params->decimation));

  if (params->decimation > 1) {
    su_iir_bwlpf_init(&new->antialias, 5, .5 / params->decimation);
  }

  return new;

fail:
  if (new != NULL)
    su_channel_detector_destroy(new);

  return NULL;
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

SUPRIVATE SUBOOL
su_channel_perform_discovery(su_channel_detector_t *detector)
{
  unsigned int i;
  unsigned int N; /* FFT size */
  unsigned int valid; /* valid FFT bins */
  unsigned int min_iters; /* minimum number of iters */
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

    min_iters = MAX(
        2. / detector->params.beta,
        2. / detector->params.alpha);

    detector_enabled = detector->iters > min_iters;
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

    /* New threshold calculated, find channels */
    if (!su_channel_detector_find_channels(detector))
      return SU_FALSE;

    su_channel_detector_channel_collect(detector);
  }

  return SU_TRUE;
}

SUBOOL
su_channel_detector_feed(su_channel_detector_t *detector, SUCOMPLEX samp)
{
  unsigned int i;
  SUCOMPLEX x;
  SUFLOAT psd;

  /* Carrier centering. Must happen *before* decimation */
  if (detector->params.mode != SU_CHANNEL_DETECTOR_MODE_DISCOVERY)
    samp *= SU_C_CONJ(su_ncqo_get(&detector->lo));

  if (detector->params.decimation > 1) {
    /* If we are decimating, we take samples from the antialias filter */
    samp = su_iir_filt_feed(&detector->antialias, samp);

    /* Decimation takes place here */
    if (++detector->decim_ptr < detector->params.decimation)
      return SU_TRUE;

    detector->decim_ptr = 0; /* Reset decimation pointer */
  }

  switch (detector->params.mode) {
    case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
      /* Channel discovery is performed on the current sample only */
      x = samp;
      break;

    case SU_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY:
      /* Baudrate estimation through cyclostationary analysis */
      x = samp * detector->prev_samp;
      detector->prev_samp = x;
      break;

    default:
      SU_WARNING("Mode not implemented\n");
      return SU_FALSE;
  }

  detector->window[detector->ptr++] = x;

  if (detector->ptr == detector->params.window_size) {
    detector->ptr = 0;

    /* Apply window function. TODO: precalculate */
    su_taps_apply_blackmann_harris_complex(
        detector->window,
        detector->params.window_size);

    /* Window is full, perform FFT */
    SU_FFTW(_execute(detector->fft_plan));

    /* Compute smooth spect */
    for (i = 0; i < detector->params.window_size; ++i) {
      psd = SU_C_REAL(detector->fft[i] * SU_C_CONJ(detector->fft[i]));
      psd /= detector->params.window_size;
      detector->spect[i] +=
          detector->params.alpha * (psd - detector->spect[i]);
    }

    switch (detector->params.mode) {
      case SU_CHANNEL_DETECTOR_MODE_DISCOVERY:
        return su_channel_perform_discovery(detector);

      case SU_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY:
        /*
         * Order estimation is based on peak detection: first peak
         * is related to baudrate
         */
        break;

      default:
        SU_WARNING("Mode not implemented\n");
        return SU_FALSE;
    }
  }

  return SU_TRUE;
}
