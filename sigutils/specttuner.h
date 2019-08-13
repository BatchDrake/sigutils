/*

  Copyright (C) 2018 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_SPECTTUNER_H
#define _SIGUTILS_SPECTTUNER_H

#include "types.h"
#include "ncqo.h"

struct sigutils_specttuner_params {
  SUSCOUNT window_size;
};

#define sigutils_specttuner_params_INITIALIZER  \
{                                               \
  4096, /* window_size */                       \
}

enum sigutils_specttuner_state {
  SU_SPECTTUNER_STATE_EVEN,
  SU_SPECTTUNER_STATE_ODD,
};

struct sigutils_specttuner_channel;

struct sigutils_specttuner_channel_params {
  SUFLOAT f0;       /* Central frequency (angular frequency) */
  SUFLOAT bw;       /* Bandwidth (angular frequency) */
  SUFLOAT guard;    /* Relative extra bandwidth */
  SUBOOL  precise;  /* Precision mode */
  void *privdata;   /* Private data */
  SUBOOL (*on_data) (
      const struct sigutils_specttuner_channel *channel,
      void *privdata,
      const SUCOMPLEX *data, /* This pointer remains valid until the next call to feed */
      SUSCOUNT size);
};

#define sigutils_specttuner_channel_params_INITIALIZER  \
{                                                       \
  0,        /* f0 */                                    \
  0,        /* bw */                                    \
  1,        /* guard */                                 \
  SU_FALSE, /* precise */                               \
  NULL,     /* private */                               \
  NULL,     /* on_data */                               \
}

struct sigutils_specttuner_channel {
  struct sigutils_specttuner_channel_params params;
  int index;           /* Back reference */

  SUFLOAT k;           /* Scaling factor */
  SUFLOAT decimation;  /* Equivalent decimation */
  su_ncqo_t lo;        /* Local oscilator to correct imprecise centering */
  unsigned int center; /* FFT center bin */
  unsigned int size;   /* FFT bins to allocate */
  unsigned int width;  /* FFT bins to copy (for guard bands, etc) */
  unsigned int halfw;  /* Half of channel width */
  unsigned int halfsz; /* Half of window size */
  unsigned int offset; /* Window offset for overlapping */

  /*
   * Again, we have to keep 2 buffers: this way we can perform
   * a good windowing that does not rely on rectangular windows
   */
  enum sigutils_specttuner_state state;
  SU_FFTW(_complex) *fft;      /* Filtered spectrum */
  SU_FFTW(_complex) *h;        /* Frequency response of filter */
  SU_FFTW(_plan)     plan[2];  /* Even & Odd plans */
  SU_FFTW(_plan)     forward;  /* Filter response forward plan */
  SU_FFTW(_plan)     backward; /* Filter response backward plan */

  SU_FFTW(_complex) *ifft[2];  /* Even & Odd time-domain signal */
  SUFLOAT           *window;   /* Window function */
};

typedef struct sigutils_specttuner_channel su_specttuner_channel_t;

SUINLINE SUFLOAT
su_specttuner_channel_get_decimation(const su_specttuner_channel_t *channel)
{
  return channel->decimation;
}

SUINLINE SUFLOAT
su_specttuner_channel_get_bw(const su_specttuner_channel_t *channel)
{
  return PI * (SUFLOAT) channel->width / (SUFLOAT) channel->size;
}

SUINLINE SUFLOAT
su_specttuner_channel_get_f0(const su_specttuner_channel_t *channel)
{
  return channel->params.f0;
}

/*
 * The spectral tuner leverages its 3/2-sized window buffer by keeping
 * two FFT plans (even & odd) and conditionally saving the same sample
 * twice.
 *
 *  <---- Even ----->
 * |________|________|________|
 *           <----- Odd ----->
 *
 * The spectral tuner changes between two states. During the even state,
 * the tuner populates the even part. During the odd statem, it populates
 * the odd part.
 *
 * |11111111|22222___|________|
 *
 * When the EVEN part is full, the even FFT plan is performed and the freq
 * domain filtering takes place. The specttuner then switches to ODD:
 *
 * |33311111|22222222|333_____|
 *
 * During the ODD state, we fill the remaining half of the odd part, but
 * also the first half of the even part. When the ODD part is full, the
 * odd plan is performed as well as the usual frequency filtering.
 */

struct sigutils_specttuner {
  struct sigutils_specttuner_params params;

  SU_FFTW(_complex) *window; /* 3/2 the space, double allocation trick */
  SU_FFTW(_complex) *fft;

  enum sigutils_specttuner_state state;
  SU_FFTW(_plan) plans[2]; /* Even and odd plans */

  unsigned int half_size; /* 3/2 of window size */
  unsigned int full_size; /* 3/2 of window size */
  unsigned int p; /* From 0 to window_size - 1 */

  unsigned int count; /* Active channels */

  SUBOOL ready; /* FFT ready */

  /* Channel list */
  PTR_LIST(struct sigutils_specttuner_channel, channel);
};

typedef struct sigutils_specttuner su_specttuner_t;

SUINLINE unsigned int
su_specttuner_get_channel_count(const su_specttuner_t *st)
{
  return st->count;
}

SUINLINE SUBOOL
su_specttuner_new_data(const su_specttuner_t *st)
{
  return st->ready;
}

SUINLINE void
su_specttuner_ack_data(su_specttuner_t *st)
{
  st->ready = SU_FALSE;
}

void su_specttuner_destroy(su_specttuner_t *st);

su_specttuner_t *su_specttuner_new(
    const struct sigutils_specttuner_params *params);

SUSDIFF su_specttuner_feed_bulk_single(
    su_specttuner_t *st,
    const SUCOMPLEX *buf,
    SUSCOUNT size);

SUBOOL su_specttuner_feed_bulk(
    su_specttuner_t *st,
    const SUCOMPLEX *buf,
    SUSCOUNT size);

su_specttuner_channel_t *su_specttuner_open_channel(
    su_specttuner_t *st,
    const struct sigutils_specttuner_channel_params *params);

void su_specttuner_set_channel_freq(
    const su_specttuner_t *st,
    su_specttuner_channel_t *channel,
    SUFLOAT f0);

SUBOOL su_specttuner_set_channel_bandwidth(
    const su_specttuner_t *st,
    su_specttuner_channel_t *channel,
    SUFLOAT bw);

SUBOOL su_specttuner_close_channel(
    su_specttuner_t *st,
    su_specttuner_channel_t *channel);

#endif /* _SIGUTILS_SPECTTUNER_H */
