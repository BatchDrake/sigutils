/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_AGC_H
#define _SIGUTILS_AGC_H

#include "defs.h"
#include "types.h"

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif  // __clang__
extern "C" {
#endif /* __cplusplus */

/*
 * This Hang AGC implementation is essentially inspired in GQRX's
 */

#define SU_AGC_RESCALE 0.7

struct sigutils_agc {
  SUBOOL enabled;

  /* AGC parameters */
  SUFLOAT knee;          /* AGC Knee in dBs */
  SUFLOAT gain_slope;    /* Gain slope in dBs (0..10) */
  SUFLOAT fixed_gain;    /* Gain below knee */
  unsigned int hang_max; /* Hang time in number of samples */
  unsigned int hang_n;   /* Hang timer */

  /* AGC memory - delay line */
  SUCOMPLEX *delay_line;
  unsigned int delay_line_size;
  unsigned int delay_line_ptr;

  /* AGC memory - signal magnitude history */
  SUFLOAT *mag_history;
  unsigned int mag_history_size;
  unsigned int mag_history_ptr;

  SUFLOAT peak; /* Current peak value in history */

  /* Used to correct transitional spikes */
  SUFLOAT fast_alpha_rise;
  SUFLOAT fast_alpha_fall;
  SUFLOAT fast_level;

  /* Used to correct a steady signal */
  SUFLOAT slow_alpha_rise;
  SUFLOAT slow_alpha_fall;
  SUFLOAT slow_level;
};

typedef struct sigutils_agc su_agc_t;

#define su_agc_INITIALIZER                                                  \
  {                                                                         \
    0, 0., 0., 0., 0, 0, NULL, 0, 0, NULL, 0, 0, 0., 0., 0., 0., 0., 0., 0. \
  }

struct su_agc_params {
  SUFLOAT threshold;
  SUFLOAT slope_factor;
  unsigned int hang_max;
  unsigned int delay_line_size;
  unsigned int mag_history_size;

  /* Time constants (in samples) for transient spikes */
  SUFLOAT fast_rise_t;
  SUFLOAT fast_fall_t;

  /* Time constants (in samples) for steady signals */
  SUFLOAT slow_rise_t;
  SUFLOAT slow_fall_t;
};

#define su_agc_params_INITIALIZER      \
  {                                    \
    -100, 6, 100, 20, 20, 2, 4, 20, 40 \
  }

SU_CONSTRUCTOR(su_agc, const struct su_agc_params *params);
SU_DESTRUCTOR(su_agc);

SU_METHOD(su_agc, SUCOMPLEX, feed, SUCOMPLEX x);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif  // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_AGC_H */
