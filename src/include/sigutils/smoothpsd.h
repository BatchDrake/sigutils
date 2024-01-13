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

#ifndef _SIGUTILS_SMOOTHPSD_H
#define _SIGUTILS_SMOOTHPSD_H

#include <pthread.h>
#include <sigutils/detect.h>
#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_smoothpsd_params {
  unsigned int fft_size;
  SUFLOAT samp_rate;
  SUFLOAT refresh_rate;
  enum sigutils_channel_detector_window window;
};

#define sigutils_smoothpsd_params_INITIALIZER                          \
  {                                                                    \
    4096,                                           /* fft_size */     \
        1e6,                                        /* samp_rate */    \
        25,                                         /* refresh_rate */ \
        SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS /* window */       \
  }

struct sigutils_smoothpsd {
  struct sigutils_smoothpsd_params params;
  SUFLOAT nominal_rate;
  pthread_mutex_t mutex;
  SUBOOL mutex_init;

  SUBOOL (*psd_func)(void *userdata, const SUFLOAT *psd, unsigned int size);
  void *userdata;

  unsigned int p;
  unsigned int fft_p;
  unsigned int max_p;

  SUSCOUNT iters;

  SU_FFTW(_complex) * window_func;
  SU_FFTW(_complex) * buffer;
  SU_FFTW(_plan) fft_plan;

  union {
    SU_FFTW(_complex) * fft;
    SUFLOAT *realfft;
  };
};

typedef struct sigutils_smoothpsd su_smoothpsd_t;

SUINLINE
SU_GETTER(su_smoothpsd, SUFLOAT, get_nominal_samp_rate)
{
  return self->nominal_rate;
}

SUINLINE
SU_METHOD(
  su_smoothpsd,
  void,
  set_nominal_samp_rate,
  SUFLOAT rate)
{
  self->nominal_rate = rate;
}

SUINLINE
SU_GETTER(su_smoothpsd, SUSCOUNT, get_iters)
{
  return self->iters;
}

SUINLINE
SU_GETTER(su_smoothpsd, unsigned int, get_fft_size)
{
  return self->params.fft_size;
}

SUINLINE
SU_GETTER(su_smoothpsd, SUFLOAT *, get_last_psd)
{
  return self->realfft;
}

SU_INSTANCER(
    su_smoothpsd,
    const struct sigutils_smoothpsd_params *params,
    SUBOOL (*psd_func)(void *userdata, const SUFLOAT *psd, unsigned int size),
    void *userdata);

SU_COLLECTOR(su_smoothpsd);

SU_METHOD(su_smoothpsd, SUBOOL, feed, const SUCOMPLEX *data, SUSCOUNT size);

SU_METHOD(
    su_smoothpsd,
    SUBOOL,
    set_params,
    const struct sigutils_smoothpsd_params *params);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_SMOOTHPSD_H */
