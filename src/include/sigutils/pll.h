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

#ifndef _SIGUTILS_PLL_H
#define _SIGUTILS_PLL_H

#include <sigutils/defs.h>
#include <sigutils/iir.h>
#include <sigutils/ncqo.h>
#include <sigutils/types.h>

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif  // __clang__
extern "C" {
#endif /* __cplusplus */

#define SU_PLL_ORDER_DEFAULT 5
#define SU_COSTAS_FIR_ORDER_THRESHOLD 20

struct sigutils_pll {
  SUFLOAT alpha;
  SUFLOAT beta;
  SUFLOAT lock;
  SUCOMPLEX a;
  su_ncqo_t ncqo;
};

typedef struct sigutils_pll su_pll_t;

#define su_pll_INITIALIZER             \
  {                                    \
    0., 0., 0., 0, su_ncqo_INITIALIZER \
  }

enum sigutils_costas_kind {
  SU_COSTAS_KIND_NONE,
  SU_COSTAS_KIND_BPSK,
  SU_COSTAS_KIND_QPSK,
  SU_COSTAS_KIND_8PSK
};

struct sigutils_costas {
  enum sigutils_costas_kind kind;
  SUFLOAT a;
  SUFLOAT b;
  SUFLOAT lock;
  su_iir_filt_t af;  /* Arm filter */
  SUCOMPLEX z;       /* Arm filter output */
  SUCOMPLEX y;       /* Demodulation result */
  SUCOMPLEX y_alpha; /* Result alpha */
  SUFLOAT gain;      /* Loop gain */
  su_ncqo_t ncqo;
};

typedef struct sigutils_costas su_costas_t;

#define su_costas_INITIALIZER                                                 \
  {                                                                           \
    SU_COSTAS_KIND_NONE, 0., 0., 0., su_iir_filt_INITIALIZER, 0., 0., 0., 1., \
        su_ncqo_INITIALIZER                                                   \
  }

/* Second order PLL */
SU_CONSTRUCTOR(su_pll, SUFLOAT fhint, SUFLOAT fc);
SU_DESTRUCTOR(su_pll);

SU_METHOD(su_pll, SUCOMPLEX, track, SUCOMPLEX x);
SU_METHOD(su_pll, void, feed, SUFLOAT x);

SUINLINE
SU_METHOD(su_pll, SUFLOAT, locksig)
{
  return self->lock;
}

/* QPSK costas loops are way more complex than that */
SU_CONSTRUCTOR(
    su_costas,
    enum sigutils_costas_kind kind,
    SUFLOAT fhint,
    SUFLOAT arm_bw,
    unsigned int arm_order,
    SUFLOAT loop_bw);
SU_DESTRUCTOR(su_costas);

SU_METHOD(su_costas, void, set_kind, enum sigutils_costas_kind kind);
SU_METHOD(su_costas, void, set_loop_gain, SUFLOAT gain);
SU_METHOD(su_costas, SUCOMPLEX, feed, SUCOMPLEX x);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif  // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_PLL_H */
