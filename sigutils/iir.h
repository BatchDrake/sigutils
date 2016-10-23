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

#ifndef _SIGUTILS_IIR_H
#define _SIGUTILS_IIR_H

#include <stdlib.h>
#include "types.h"

#define SU_FLOAT_GUARD INFINITY

/* TODO: Builtin filters */
struct sigutils_iir_filt {
  unsigned int x_size;
  unsigned int y_size;

  int x_ptr;
  int y_ptr;

  SUFLOAT *y;
  SUFLOAT *x;

  SUFLOAT *a;
  SUFLOAT *b;
};

typedef struct sigutils_iir_filt su_iir_filt_t;

#define su_iir_filt_INITIALIZER {0, 0, 0, 0, NULL, NULL, NULL, NULL }

/* Push sample to filter */
SUFLOAT su_iir_filt_feed(su_iir_filt_t *filt, SUFLOAT x);

/* Get last output */
SUFLOAT su_iir_filt_get(const su_iir_filt_t *filt);

/* Initialize filter */
SUBOOL su_iir_filt_init(
    su_iir_filt_t *filt,
    unsigned int y_size,
    const SUFLOAT *a,
    unsigned int x_size,
    const SUFLOAT *b);

/* Initialize Butterworth low-pass filter of order N */
SUBOOL su_iir_bwlpf_init(su_iir_filt_t *filt, unsigned int n, SUFLOAT fc);

/* Destroy filter */
void su_iir_filt_finalize(su_iir_filt_t *filt);

#endif
