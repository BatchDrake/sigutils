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

#ifndef _SIGUTILS_SPECTTUNER_H
#define _SIGUTILS_SPECTTUNER_H

#include "defs.h"
#include "ncqo.h"
#include "types.h"

#ifdef __cplusplus
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif  // __clang__
extern "C" {
#endif /* __cplusplus */

struct sigutils_specttuner_params {
  SUSCOUNT window_size;
};

#define sigutils_specttuner_params_INITIALIZER \
  {                                            \
    4096, /* window_size */                    \
  }

enum sigutils_specttuner_state {
  SU_SPECTTUNER_STATE_EVEN,
  SU_SPECTTUNER_STATE_ODD,
};

struct sigutils_specttuner_channel;

struct sigutils_specttuner_channel_params {
  SUFLOAT f0;      /* Central frequency (angular frequency) */
  SUFLOAT delta_f; /* Frequency correction (angular frequency) */
  SUFLOAT bw;      /* Bandwidth (angular frequency) */
  SUFLOAT guard;   /* Relative extra bandwidth */
  SUBOOL precise;  /* Precision mode */
  void *privdata;  /* Private data */
  SUBOOL(*on_data)
  (const struct sigutils_specttuner_channel *channel,
   void *privdata,
   const SUCOMPLEX
       *data, /* This pointer remains valid until the next call to feed */
   SUSCOUNT size);
};

#define sigutils_specttuner_channel_params_INITIALIZER \
  {                                                    \
    0,            /* f0 */                             \
        0,        /* delta_f */                        \
        0,        /* bw */                             \
        1,        /* guard */                          \
        SU_FALSE, /* precise */                        \
        NULL,     /* private */                        \
        NULL,     /* on_data */                        \
  }

struct sigutils_specttuner_channel {
  struct sigutils_specttuner_channel_params params;
  int index; /* Back reference */

  SUFLOAT k;           /* Scaling factor */
  SUFLOAT gain;        /* Channel gain */
  SUFLOAT decimation;  /* Equivalent decimation */
  su_ncqo_t lo;        /* Local oscillator to correct imprecise centering */
  su_ncqo_t old_lo;    /* Copy of the old local oscillator */
  SUBOOL pending_freq; /* Pending frequency adjustment */
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
  SU_FFTW(_complex) * fft; /* Filtered spectrum */
  SU_FFTW(_complex) * h;   /* Frequency response of filter */
  SU_FFTW(_plan) plan[2];  /* Even & Odd plans */
  SU_FFTW(_plan) forward;  /* Filter response forward plan */
  SU_FFTW(_plan) backward; /* Filter response backward plan */

  SU_FFTW(_complex) * ifft[2]; /* Even & Odd time-domain signal */
  SUFLOAT *window;             /* Window function */
};

typedef struct sigutils_specttuner_channel su_specttuner_channel_t;

SUINLINE
SU_GETTER(su_specttuner_channel, SUFLOAT, get_decimation)
{
  return self->decimation;
}

SUINLINE
SU_GETTER(su_specttuner_channel, SUFLOAT, get_bw)
{
  return 2 * PI * (SUFLOAT)self->width
         / (SUFLOAT)(self->size * self->decimation);
}

SUINLINE
SU_GETTER(su_specttuner_channel, SUFLOAT, get_f0)
{
  return self->params.f0;
}

SUINLINE
SU_GETTER(su_specttuner_channel, SUFLOAT, get_delta_f)
{
  return self->params.delta_f;
}

SUINLINE
SU_GETTER(su_specttuner_channel, SUFLOAT, get_effective_freq)
{
  SUFLOAT ef = SU_FMOD(self->params.f0 + self->params.delta_f, 2 * M_PI);

  if (ef < 0)
    ef += 2 * PI;

  return ef;
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

  SU_FFTW(_complex) * window; /* 3/2 the space, double allocation trick */
  SU_FFTW(_complex) * fft;

  enum sigutils_specttuner_state state;
  SU_FFTW(_plan) plans[2]; /* Even and odd plans */

  unsigned int half_size; /* 3/2 of window size */
  unsigned int full_size; /* 3/2 of window size */
  unsigned int p;         /* From 0 to window_size - 1 */

  unsigned int count; /* Active channels */

  SUBOOL ready; /* FFT ready */

  /* Channel list */
  PTR_LIST(struct sigutils_specttuner_channel, channel);
};

typedef struct sigutils_specttuner su_specttuner_t;

SUINLINE
SU_GETTER(su_specttuner, unsigned int, get_channel_count)
{
  return self->count;
}

SUINLINE
SU_GETTER(su_specttuner, SUBOOL, new_data)
{
  return self->ready;
}

SUINLINE
SU_METHOD(su_specttuner, void, ack_data)
{
  self->ready = SU_FALSE;
}

#ifndef __cplusplus
/* Internal */
SU_METHOD(su_specttuner, SUBOOL, feed_all_channels);

SUINLINE
SU_METHOD(su_specttuner, SUBOOL, feed_sample, SUCOMPLEX x)
{
  SUSDIFF halfsz = self->half_size;
  SUSDIFF p = self->p;
  SUBOOL ready = SU_FALSE;

  switch (self->state) {
    case SU_SPECTTUNER_STATE_EVEN:
      /* Just copy at the beginning */
      self->window[p] = x;
      break;

    case SU_SPECTTUNER_STATE_ODD:
      /* Copy to the second third */
      self->window[p + halfsz] = x;

      /* Are we populating the last third too? */
      if (p >= halfsz)
        self->window[p - halfsz] = x;
  }

  if (++p < self->params.window_size) {
    self->p = p;
  } else {
    self->p = halfsz;

    /* Compute FFT */
    SU_FFTW(_execute)(self->plans[self->state]);

    /* Toggle state */
    self->state = !self->state;

    ready = SU_TRUE;
  }

  return ready;
}
#endif /* __cplusplus */

SU_INSTANCER(su_specttuner, const struct sigutils_specttuner_params *params);
SU_COLLECTOR(su_specttuner);

SU_METHOD(su_specttuner,
          SUSDIFF,
          feed_bulk_single,
          const SUCOMPLEX *__restrict buf,
          SUSCOUNT size);

SU_METHOD(su_specttuner,
          SUBOOL,
          feed_bulk,
          const SUCOMPLEX *__restrict buf,
          SUSCOUNT size);

SU_METHOD(su_specttuner,
          su_specttuner_channel_t *,
          open_channel,
          const struct sigutils_specttuner_channel_params *params);

SU_METHOD(su_specttuner,
          SUBOOL,
          close_channel,
          su_specttuner_channel_t *channel);

SU_METHOD_CONST(su_specttuner,
                void,
                set_channel_freq,
                su_specttuner_channel_t *channel,
                SUFLOAT f0);

SU_METHOD_CONST(su_specttuner,
                void,
                set_channel_delta_f,
                su_specttuner_channel_t *channel,
                SUFLOAT delta_f);

SU_METHOD_CONST(su_specttuner,
                SUBOOL,
                set_channel_bandwidth,
                su_specttuner_channel_t *channel,
                SUFLOAT bw);

#ifdef __cplusplus
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_SPECTTUNER_H */
