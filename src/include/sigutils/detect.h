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

#ifndef _SIGUTILS_DETECT_H
#define _SIGUTILS_DETECT_H

#include <sigutils/iir.h>
#include <sigutils/ncqo.h>
#include <sigutils/sigutils.h>
#include <sigutils/softtune.h>


#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif  // __clang__
extern "C" {
#endif /* __cplusplus */

#define SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE 0    /* in FFT runs */
#define SU_CHANNEL_DETECTOR_MIN_SNR SU_ADDSFX(6.) /* in DBs */
#define SU_CHANNEL_DETECTOR_MIN_BW SU_ADDSFX(10.) /* in Hz */

#define SU_CHANNEL_DETECTOR_ALPHA SU_ADDSFX(1e-2)
#define SU_CHANNEL_DETECTOR_BETA SU_ADDSFX(1e-3)
#define SU_CHANNEL_DETECTOR_GAMMA SU_ADDSFX(0.5)
#define SU_CHANNEL_MAX_AGE 40 /* In FFT runs */
#define SU_CHANNEL_DETECTOR_PEAK_PSD_ALPHA SU_ADDSFX(.25)
#define SU_CHANNEL_DETECTOR_DC_ALPHA SU_ADDSFX(0.1)
#define SU_CHANNEL_DETECTOR_AVG_TIME_WINDOW SU_ADDSFX(10.) /* In seconds */

#define SU_CHANNEL_IS_VALID(cp)                     \
  ((cp)->age > SU_CHANNEL_DETECTOR_MIN_MAJORITY_AGE \
   && (cp)->snr > SU_CHANNEL_DETECTOR_MIN_SNR       \
   && (cp)->bw > SU_CHANNEL_DETECTOR_MIN_BW)

#define SU_CHANNEL_DETECTOR_IDX2ABS_FREQ(detector, i)              \
  SU_NORM2ABS_FREQ(                                                \
      (detector)->params.samp_rate *(detector)->params.decimation, \
      2 * (SUFLOAT)(i) / (SUFLOAT)(detector)->params.window_size)

struct sigutils_peak_detector {
  unsigned int size;
  SUFLOAT thr2; /* In sigmas */

  /* State */
  SUFLOAT *history;
  unsigned int p;
  unsigned int count;
  SUFLOAT accum;    /* Scaled mean */
  SUFLOAT inv_size; /* 1. / size */
};

typedef struct sigutils_peak_detector su_peak_detector_t;

#define su_peak_detector_INITIALIZER \
  {                                  \
    0,        /* size */             \
        0,    /* thr2 */             \
        NULL, /* history */          \
        0,    /* p */                \
        0,    /* count */            \
        0,    /* accum */            \
        0,    /* inv_size */         \
  }

enum sigutils_channel_detector_mode {
  SU_CHANNEL_DETECTOR_MODE_SPECTRUM,        /* Spectrum only mode */
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
  SUFLOAT fc;           /* Center frequency */
  SUSCOUNT decimation;  /* Decimation */
  SUFLOAT bw;           /* Low-pass filter bandwidth (in Hz) */
  SUSCOUNT max_order;   /* Max constellation order */
  SUBOOL tune;          /* Signal needs to be tuned to a channel */

  /* Detector parameters */
  enum sigutils_channel_detector_window window; /* Window function */
  SUFLOAT alpha;                                /* PSD averaging ratio */
  SUFLOAT beta;     /* PSD upper and lower levels averaging ratio  */
  SUFLOAT gamma;    /* Noise level update ratio */
  SUFLOAT snr;      /* Minimum SNR to detect channels (linear) */
  SUSCOUNT max_age; /* Max channel age */

  /* Peak detector parameters */
  SUSCOUNT pd_size;  /* PD samples */
  SUFLOAT pd_thres;  /* PD threshold, in sigmas */
  SUFLOAT pd_signif; /* Minimum significance, in dB */
};

#define sigutils_channel_detector_params_INITIALIZER                   \
  {                                                                    \
    SU_CHANNEL_DETECTOR_MODE_SPECTRUM,               /* Mode */        \
        8000,                                        /* samp_rate */   \
        8192,                                        /* window_size */ \
        SU_ADDSFX(0.0),                              /* fc */          \
        1,                                           /* decimation */  \
        SU_ADDSFX(0.0),                              /* bw */          \
        8,                                           /* max_order */   \
        SU_FALSE,                                    /* tune */        \
        SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS, /* window */      \
        SU_CHANNEL_DETECTOR_ALPHA,                   /* alpha */       \
        SU_CHANNEL_DETECTOR_BETA,                    /* beta */        \
        SU_CHANNEL_DETECTOR_GAMMA,                   /* gamma */       \
        SU_ADDSFX(2.),                               /* snr */         \
        SU_CHANNEL_MAX_AGE,                          /* max_age */     \
        10,                                          /* pd_samples */  \
        SU_ADDSFX(2.),                               /* pd_thres */    \
        SU_ADDSFX(10.)                               /* pd_signif */   \
  }

#define sigutils_channel_INITIALIZER \
  {                                  \
    0,     /* fc */                  \
        0, /* f_lo */                \
        0, /* f_li */                \
        0, /* bw */                  \
        0, /* snr */                 \
        0, /* S0 */                  \
        0, /* N0 */                  \
        0, /* ft */                  \
        0, /* age */                 \
        0, /* present */             \
  }

struct sigutils_channel_detector {
  /* Common members */
  struct sigutils_channel_detector_params params;
  su_softtuner_t tuner;
  SUCOMPLEX *tuner_buf;
  SUSCOUNT ptr; /* Sample in window */
  SUBOOL fft_issued;
  SUSCOUNT next_to_window;
  unsigned int iters;
  unsigned int chan_age;
  SU_FFTW(_complex) * window_func;
  SU_FFTW(_complex) * window;
  SU_FFTW(_plan) fft_plan;
  SU_FFTW(_complex) * fft;
  SUSCOUNT req_samples; /* Number of required samples for detection */

  union {
    SUFLOAT *spect; /* Used only if mode == DISCOVERY, NONLINEAR_DIFF */
    SUFLOAT *acorr; /* Used only if mode == AUTOCORRELATION */
    void *_r_alloc; /* Generic allocation */
  };

  /* Channel detector members */
  SU_FFTW(_plan) fft_plan_rev;
  SU_FFTW(_complex) * ifft;
  SUFLOAT *spmax;
  SUFLOAT *spmin;
  SUFLOAT N0;   /* Detected noise floor */
  SUCOMPLEX dc; /* Detected DC component */
  PTR_LIST(struct sigutils_channel, channel);

  /* Baudrate estimator members */
  SUFLOAT baud;          /* Detected baudrate */
  SUCOMPLEX prev;        /* Used by nonlinear diff */
  su_peak_detector_t pd; /* Peak detector used by nonlinear diff */
};

typedef struct sigutils_channel_detector su_channel_detector_t;

SUINLINE
SU_METHOD(su_channel_detector, void, rewind)
{
  self->ptr = 0;
  self->iters = 0;
}

SUINLINE
SU_GETTER(su_channel_detector, unsigned int, get_iters)
{
  return self->iters;
}

SUINLINE
SU_GETTER(su_channel_detector, SUCOMPLEX, get_dc)
{
  return self->dc;
}

SUINLINE
SU_GETTER(su_channel_detector, SUSCOUNT, get_fs)
{
  return self->params.samp_rate;
}

SUINLINE
SU_GETTER(su_channel_detector, SUSCOUNT, get_window_ptr)
{
  return self->ptr;
}

SUINLINE
SU_GETTER(su_channel_detector, SUFLOAT, get_baud)
{
  return self->baud;
}

SUINLINE
SU_GETTER(su_channel_detector, SUSCOUNT, get_window_size)
{
  return self->params.window_size;
}

/**************************** Peak detector API *****************************/
SU_CONSTRUCTOR(su_peak_detector, unsigned int size, SUFLOAT thres);
SU_DESTRUCTOR(su_peak_detector);

SU_METHOD(su_peak_detector, int, feed, SUFLOAT x);

/************************** Channel detector API ****************************/
SU_INSTANCER(
    su_channel_detector,
    const struct sigutils_channel_detector_params *params);
SU_COLLECTOR(su_channel_detector);

SU_METHOD(
    su_channel_detector,
    SUBOOL,
    set_params,
    const struct sigutils_channel_detector_params *params);

SU_GETTER(su_channel_detector, SUSCOUNT, get_req_samples);

SU_METHOD(su_channel_detector, SUBOOL, feed, SUCOMPLEX x);

SU_METHOD(
    su_channel_detector,
    SUSCOUNT,
    feed_bulk,
    const SUCOMPLEX *signal,
    SUSCOUNT size);

SU_METHOD(su_channel_detector, SUBOOL, exec_fft);

SU_GETTER(
    su_channel_detector,
    void,
    get_channel_list,
    struct sigutils_channel ***channel_list,
    unsigned int *channel_count);

void su_channel_params_adjust(struct sigutils_channel_detector_params *params);

void su_channel_params_adjust_to_channel(
    struct sigutils_channel_detector_params *params,
    const struct sigutils_channel *channel);

SU_GETTER(su_channel_detector, su_channel_t *, lookup_channel, SUFLOAT fc);
SU_GETTER(
    su_channel_detector,
    su_channel_t *,
    lookup_valid_channel,
    SUFLOAT fc);

SU_COPY_INSTANCER(su_channel);
SU_COLLECTOR(su_channel);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif  // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_DETECT_H */
