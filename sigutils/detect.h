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

#ifndef _SIGUTILS_DETECT_H
#define _SIGUTILS_DETECT_H

#include "sigutils.h"
#include "ncqo.h"
#include "iir.h"
#include "softtune.h"

#define SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE 0  /* in FFT runs */
#define SU_CHANNEL_DETECTOR_MIN_SNR          6  /* in DBs */
#define SU_CHANNEL_DETECTOR_MIN_BW           10 /* in Hz */

#define SU_CHANNEL_DETECTOR_ALPHA            1e-2
#define SU_CHANNEL_DETECTOR_BETA             1e-3
#define SU_CHANNEL_DETECTOR_GAMMA            .5
#define SU_CHANNEL_MAX_AGE                   40 /* In FFT runs */
#define SU_CHANNEL_DETECTOR_PEAK_PSD_ALPHA   .25

#define SU_CHANNEL_DETECTOR_AVG_TIME_WINDOW  10. /* In seconds */

#define SU_CHANNEL_IS_VALID(cp)                               \
        ((cp)->age > SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE     \
      && (cp)->snr > SU_CHANNEL_DETECTOR_MIN_SNR              \
      && (cp)->bw  > SU_CHANNEL_DETECTOR_MIN_BW)

#define SU_CHANNEL_DETECTOR_IDX2ABS_FREQ(detector, i)                       \
    SU_NORM2ABS_FREQ(                                                       \
              (detector)->params.samp_rate * (detector)->params.decimation, \
              2 * (SUFLOAT) (i) / (SUFLOAT) (detector)->params.window_size)

struct sigutils_peak_detector {
  unsigned int size;
  SUFLOAT thr2; /* In sigmas */

  /* State */
  SUFLOAT *history;
  unsigned int p;
  unsigned int count;
  SUFLOAT accum; /* Scaled mean */
  SUFLOAT inv_size; /* 1. / size */
};

typedef struct sigutils_peak_detector su_peak_detector_t;

#define su_peak_detector_INITIALIZER    \
{                                       \
  0, /* size */                         \
  0, /* thr2 */                         \
  NULL, /* history */                   \
  0, /* p */                            \
  0, /* count */                        \
  0, /* accum */                        \
  0, /* inv_size */                     \
}

enum sigutils_channel_detector_mode {
  SU_CHANNEL_DETECTOR_MODE_DISCOVERY,       /* Discover channels */
  SU_CHANNEL_DETECTOR_MODE_AUTOCORRELATION, /* To find baudrate */
  SU_CHANNEL_DETECTOR_MODE_NONLINEAR_DIFF,  /* To find baudrate (alt.) */
  SU_CHANNEL_DETECTOR_MODE_ORDER_ESTIMATION /* To find constellation size */
};

enum sigutils_channel_detector_window {
  SU_CHANNEL_DETECTOR_WINDOW_NONE,             /* Rectangular window */
  SU_CHANNEL_DETECTOR_WINDOW_HAMMING,          /* Hamming window */
  SU_CHANNEL_DETECTOR_WINDOW_HANN,             /* Hann window */
  SU_CHANNEL_DETECTOR_WINDOW_FLAT_TOP,         /* Flat-top window */
  SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS, /* Blackmann-Harris window */
};

struct sigutils_channel_detector_params {
  enum sigutils_channel_detector_mode mode;
  SUSCOUNT samp_rate;   /* Sample rate */
  SUSCOUNT window_size; /* Window size == FFT bins */
  SUFLOAT  fc;          /* Center frequency */
  SUSCOUNT decimation;  /* Decimation */
  SUFLOAT  bw;          /* Low-pass filter bandwidth (in Hz) */
  SUSCOUNT max_order;   /* Max constellation order */
  SUBOOL   tune;        /* Signal needs to be tuned to a channel */

  /* Detector parameters */
  enum sigutils_channel_detector_window window; /* Window function */
  SUFLOAT  alpha;        /* PSD averaging ratio */
  SUFLOAT  beta;         /* PSD upper and lower levels averaging ratio  */
  SUFLOAT  gamma;        /* Noise level update ratio */
  SUFLOAT  snr;          /* Minimum SNR to detect channels (linear) */
  SUSCOUNT max_age;      /* Max channel age */

  /* Peak detector parameters */
  SUSCOUNT pd_size;   /* PD samples */
  SUFLOAT  pd_thres;  /* PD threshold, in sigmas */
  SUFLOAT  pd_signif; /* Minimum significance, in dB */
};

#define sigutils_channel_detector_params_INITIALIZER            \
{                                                               \
  SU_CHANNEL_DETECTOR_MODE_DISCOVERY, /* Mode */                \
  8000,     /* samp_rate */                                     \
  512,      /* window_size */                                   \
  0.0,      /* fc */                                            \
  1,        /* decimation */                                    \
  0.0,      /* bw */                                            \
  8,        /* max_order */                                     \
  SU_TRUE,  /* tune */                                          \
  SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS, /* window */     \
  SU_CHANNEL_DETECTOR_ALPHA, /* alpha */                        \
  SU_CHANNEL_DETECTOR_BETA,  /* beta */                         \
  SU_CHANNEL_DETECTOR_GAMMA, /* gamma */                        \
  2,        /* snr */                                           \
  SU_CHANNEL_MAX_AGE,        /* max_age */                      \
  10,       /* pd_samples */                                    \
  2.,       /* pd_thres */                                      \
  10        /* pd_signif */                                     \
}

#define sigutils_channel_INITIALIZER    \
{                                       \
  0,    /* fc */                        \
  0,    /* f_lo */                      \
  0,    /* f_li */                      \
  0,    /* bw */                        \
  0,    /* snr */                       \
  0,    /* S0 */                        \
  0,    /* N0 */                        \
  0,    /* ft */                        \
  0,    /* age */                       \
  0,    /* present */                   \
}

struct sigutils_channel_detector {
  /* Common members */
  struct sigutils_channel_detector_params params;
  su_softtuner_t tuner;
  SUCOMPLEX *tuner_buf;
  SUSCOUNT ptr; /* Sample in window */
  unsigned int iters;
  unsigned int chan_age;
  SU_FFTW(_complex) *window_func;
  SU_FFTW(_complex) *window;
  SU_FFTW(_plan) fft_plan;
  SU_FFTW(_complex) *fft;
  SUSCOUNT req_samples; /* Number of required samples for detection */

  union {
    SUFLOAT *spect; /* Used only if mode == DISCOVERY, NONLINEAR_DIFF */
    SUFLOAT *acorr; /* Used only if mode == AUTOCORRELATION */
    void *_r_alloc; /* Generic allocation */
  };

  /* Channel detector members */
  SU_FFTW(_plan) fft_plan_rev;
  SU_FFTW(_complex) *ifft;
  SUFLOAT *spmax;
  SUFLOAT *spmin;
  SUFLOAT N0; /* Detected noise floor */
  PTR_LIST(struct sigutils_channel, channel);

  /* Baudrate estimator members */
  SUFLOAT baud; /* Detected baudrate */
  SUCOMPLEX prev; /* Used by nonlinear diff */
  su_peak_detector_t pd; /* Peak detector used by nonlinear diff */
};

typedef struct sigutils_channel_detector su_channel_detector_t;

SUINLINE SUSCOUNT
su_channel_detector_get_fs(const su_channel_detector_t *cd)
{
  return cd->params.samp_rate;
}


SUINLINE SUBOOL
su_channel_detector_get_window_ptr(const su_channel_detector_t *cd)
{
  return cd->ptr;
}

SUINLINE SUFLOAT
su_channel_detector_get_baud(const su_channel_detector_t *cd)
{
  return cd->baud;
}

/**************************** Peak detector API *****************************/
SUBOOL su_peak_detector_init(
    su_peak_detector_t *pd,
    unsigned int size,
    SUFLOAT thres);

int su_peak_detector_feed(su_peak_detector_t *pd, SUFLOAT x);

void su_peak_detector_finalize(su_peak_detector_t *pd);

/************************** Channel detector API ****************************/
su_channel_detector_t *su_channel_detector_new(
    const struct sigutils_channel_detector_params *params);

SUBOOL su_channel_detector_set_params(
    su_channel_detector_t *detector,
    const struct sigutils_channel_detector_params *params);

SUSCOUNT su_channel_detector_get_req_samples(
    const su_channel_detector_t *detector);

void su_channel_detector_destroy(su_channel_detector_t *detector);

SUBOOL su_channel_detector_feed(
    su_channel_detector_t *detector,
    SUCOMPLEX x);

SUSCOUNT su_channel_detector_feed_bulk(
    su_channel_detector_t *detector,
    const SUCOMPLEX *signal,
    SUSCOUNT size);

void su_channel_detector_get_channel_list(
    const su_channel_detector_t *detector,
    struct sigutils_channel ***channel_list,
    unsigned int *channel_count);

void su_channel_params_adjust(struct sigutils_channel_detector_params *params);

void su_channel_params_adjust_to_channel(
    struct sigutils_channel_detector_params *params,
    const struct sigutils_channel *channel);

struct sigutils_channel *su_channel_dup(const struct sigutils_channel *channel);

void su_channel_destroy(struct sigutils_channel *channel);

struct sigutils_channel *su_channel_detector_lookup_channel(
    const su_channel_detector_t *detector,
    SUFLOAT fc);

struct sigutils_channel *su_channel_detector_lookup_valid_channel(
    const su_channel_detector_t *detector,
    SUFLOAT fc);

#endif /* _SIGUTILS_DETECT_H */

