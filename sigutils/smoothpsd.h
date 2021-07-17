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

#include <sigutils/types.h>
#include <sigutils/detect.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_smoothpsd_params {
  unsigned int fft_size;
  SUFLOAT samp_rate;
  SUFLOAT refresh_rate;
  enum sigutils_channel_detector_window window;
};

#define sigutils_smoothpsd_params_INITIALIZER       \
{                                                   \
  4096, /* fft_size */                              \
  1e6, /* samp_rate */                              \
  25, /* refresh_rate */                            \
  SU_CHANNEL_DETECTOR_WINDOW_BLACKMANN_HARRIS /* window */ \
}

struct sigutils_smoothpsd {
  struct sigutils_smoothpsd_params params;
  pthread_mutex_t mutex;
  SUBOOL          mutex_init;

  SUBOOL (*psd_func) (void *userdata, const SUFLOAT *psd, unsigned int size);
  void *userdata;

  unsigned int p;
  unsigned int fft_p;
  unsigned int max_p;

  SUSCOUNT iters;

  SU_FFTW(_complex) *window_func;
  SU_FFTW(_complex) *buffer;
  SU_FFTW(_plan) fft_plan;

  union {
    SU_FFTW(_complex) *fft;
    SUFLOAT *realfft;
  };
};

typedef struct sigutils_smoothpsd su_smoothpsd_t;

SUINLINE SUSCOUNT
su_smoothpsd_get_iters(const su_smoothpsd_t *self)
{
  return self->iters;
}

SUINLINE unsigned int
su_smoothpsd_get_fft_size(const su_smoothpsd_t *self)
{
  return self->params.fft_size;
}


SUINLINE SUFLOAT *
su_smoothpsd_get_last_psd(const su_smoothpsd_t *self)
{
  return self->realfft;
}

su_smoothpsd_t *su_smoothpsd_new(
    const struct sigutils_smoothpsd_params *params,
    SUBOOL (*psd_func) (void *userdata, const SUFLOAT *psd, unsigned int size),
    void *userdata);

SUBOOL su_smoothpsd_feed(
    su_smoothpsd_t *self,
    const SUCOMPLEX *data,
    SUSCOUNT size);

SUBOOL su_smoothpsd_set_params(
    su_smoothpsd_t *self,
    const struct sigutils_smoothpsd_params *params);

void su_smoothpsd_destroy(su_smoothpsd_t *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_SMOOTHPSD_H */

