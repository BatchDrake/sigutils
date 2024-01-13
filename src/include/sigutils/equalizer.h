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

#ifndef _SIGUTILS_EQUALIZER_H
#define _SIGUTILS_EQUALIZER_H

#include <sigutils/types.h>

enum sigutils_equalizer_algorithm {
  SU_EQUALIZER_ALGORITHM_CMA, /* Default */
};

struct sigutils_equalizer_params {
  enum sigutils_equalizer_algorithm algorithm;
  SUSCOUNT length;
  SUFLOAT mu;
};

#define sigutils_equalizer_params_INITIALIZER   \
  {                                             \
    SU_EQUALIZER_ALGORITHM_CMA, /* algorithm */ \
        10,                     /* length */    \
        0.2,                    /* mu */        \
  }

/*
 * A signal equalizer is basically an adaptive filter, so we can leverage
 * the existing su_iir_filt API
 */

struct sigutils_equalizer {
  struct sigutils_equalizer_params params;
  SUCOMPLEX *w;
  SUCOMPLEX *x;
  SUSCOUNT ptr;
};

typedef struct sigutils_equalizer su_equalizer_t;

#define su_equalizer_INITIALIZER                        \
  {                                                     \
    sigutils_equalizer_params_INITIALIZER, /* params */ \
        NULL,                              /* w */      \
        NULL,                              /* x */      \
        0,                                 /* ptr */    \
  }

SUBOOL su_equalizer_init(
    su_equalizer_t *eq,
    const struct sigutils_equalizer_params *params);

void su_equalizer_reset(su_equalizer_t *eq);

SUCOMPLEX su_equalizer_feed(su_equalizer_t *eq, SUCOMPLEX x);

void su_equalizer_finalize(su_equalizer_t *eq);

#endif /* _SIGUTILS_EQUALIZER_H */
