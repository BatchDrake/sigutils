/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_PLL_H
#define _SIGUTILS_PLL_H

#include "types.h"
#include "iir.h"
#include "ncqo.h"

#define SU_PLL_ORDER_DEFAULT 5
#define SU_COSTAS_FIR_ORDER_THRESHOLD 20

struct sigutils_pll {
  SUFLOAT   alpha;
  SUFLOAT   beta;
  SUFLOAT   lock;
  SUCOMPLEX a;
  su_ncqo_t ncqo;
};

typedef struct sigutils_pll su_pll_t;

#define su_pll_INITIALIZER {0., 0., 0., 0, su_ncqo_INITIALIZER}

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
  su_iir_filt_t af; /* Arm filter */
  SUCOMPLEX z; /* Arm filter output */
  SUCOMPLEX y; /* Demodulation result */
  SUCOMPLEX y_alpha; /* Result alpha */
  SUFLOAT gain; /* Loop gain */
  su_ncqo_t ncqo;
};

typedef struct sigutils_costas su_costas_t;

#define su_costas_INITIALIZER   \
{                               \
  SU_COSTAS_KIND_NONE,          \
  0.,                           \
  0.,                           \
  0.,                           \
  su_iir_filt_INITIALIZER,      \
  0.,                           \
  0.,                           \
  0.,                           \
  1.,                           \
  su_ncqo_INITIALIZER           \
}

/* Second order PLL */
void su_pll_finalize(su_pll_t *);
SUBOOL su_pll_init(su_pll_t *, SUFLOAT, SUFLOAT);
void su_pll_feed(su_pll_t *, SUFLOAT);

/* QPSK costas loops are way more complex than that */
void su_costas_finalize(su_costas_t *);

SUBOOL su_costas_init(
    su_costas_t *costas,
    enum sigutils_costas_kind kind,
    SUFLOAT fhint,
    SUFLOAT arm_bw,
    unsigned int arm_order,
    SUFLOAT loop_bw);

void su_costas_set_loop_gain(su_costas_t *costas, SUFLOAT gain);

SUCOMPLEX su_costas_feed(su_costas_t *costas, SUCOMPLEX x);

#endif /* _SIGUTILS_PLL_H */
