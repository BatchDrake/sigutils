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

#define SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE 20 /* in FFT runs */
#define SU_CHANNEL_DETECTOR_MIN_SNR          6  /* in DBs */
#define SU_CHANNEL_DETECTOR_MIN_BW           10 /* in Hz */

#define SU_CHANNEL_IS_VALID(cp)                               \
        ((cp)->age > SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE     \
      && (cp)->snr > SU_CHANNEL_DETECTOR_MIN_SNR              \
      && (cp)->bw  > SU_CHANNEL_DETECTOR_MIN_BW)

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
  SU_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY, /* To find baudrate */
  SU_CHANNEL_DETECTOR_MODE_ORDER_ESTIMATION /* To find constellation size */
};

struct sigutils_channel_detector_params {
  enum sigutils_channel_detector_mode mode;
  unsigned int samp_rate;   /* Sample rate */
  unsigned int window_size; /* Window size == FFT bins */
  SUFLOAT fc;               /* Center frequency */
  unsigned int decimation;
  unsigned int max_order;   /* Max constellation order */

  /* Detector parameters */
  SUFLOAT alpha;            /* FFT averaging ratio */
  SUFLOAT beta;             /* Signal & Noise level update ratio */
  SUFLOAT rel_squelch;      /* Relative squelch level */
  SUFLOAT th_alpha;         /* Threshold level averaging ratio */
};

#define su_channel_detector_params_INITIALIZER \
{                         \
  SU_CHANNEL_DETECTOR_MODE_DISCOVERY, /* Mode */ \
  8000, /* samp_rate */   \
  512,  /* window_size */ \
  0.0,  /* fc */          \
  1,    /* decimation */  \
  8,    /* max_order */   \
  0.25, /* alpha */       \
  0.25, /* beta */        \
  0.3,  /* rel_squelch */ \
  .5,   /* th_alpha */    \
}


struct sigutils_channel {
  SUFLOAT fc;
  SUFLOAT bw;
  SUFLOAT snr;
  unsigned int age;
  unsigned int present;
};

struct sigutils_channel_detector {
  struct sigutils_channel_detector_params params;
  su_ncqo_t lo; /* Local oscillator */
  su_iir_filt_t antialias; /* Antialiasing filter */
  SU_FFTW(_complex) *window;
  SU_FFTW(_plan) fft_plan;
  SU_FFTW(_complex) *fft;
  SUFLOAT *averaged_fft;
  SUFLOAT *threshold;
  unsigned int decim_ptr;
  unsigned int ptr; /* Sample in window */
  unsigned int iters;
  PTR_LIST(struct sigutils_channel, channel);
};

typedef struct sigutils_channel_detector su_channel_detector_t;

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

void su_channel_detector_destroy(su_channel_detector_t *detector);

SUBOOL su_channel_detector_feed(
    su_channel_detector_t *detector,
    SUCOMPLEX samp);

void su_channel_detector_get_channel_list(
    const su_channel_detector_t *detector,
    struct sigutils_channel ***channel_list,
    unsigned int *channel_count);

struct sigutils_channel *su_channel_detector_lookup_channel(
    const su_channel_detector_t *detector,
    SUFLOAT fc);

#endif /* _SIGUTILS_DETECT_H */

