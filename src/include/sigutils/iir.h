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

#ifndef _SIGUTILS_IIR_H
#define _SIGUTILS_IIR_H

#include <stdlib.h>

#include <sigutils/types.h>

#define SU_FLOAT_GUARD INFINITY

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif  // __clang__
extern "C" {
#endif /* __cplusplus */

/* TODO: Builtin filters */
struct sigutils_iir_filt {
  unsigned int x_size;
  unsigned int y_size;

  unsigned int x_alloc;
  unsigned int y_alloc;

  int x_ptr;
  int y_ptr;

  SUCOMPLEX curr_y;

  SUCOMPLEX *y;
  SUCOMPLEX *x;

  SUFLOAT *a;
  SUFLOAT *b;

  SUFLOAT gain;
};

typedef struct sigutils_iir_filt su_iir_filt_t;

#define su_iir_filt_INITIALIZER                    \
  {                                                \
    0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 1 \
  }

/* Push sample to filter */
SUCOMPLEX su_iir_filt_feed(su_iir_filt_t *filt, SUCOMPLEX x);

/* Push a bunch of samples to filter */
void su_iir_filt_feed_bulk(
    su_iir_filt_t *filt,
    const SUCOMPLEX *__restrict x,
    SUCOMPLEX *__restrict y,
    SUSCOUNT len);

/* Get last output */
SUCOMPLEX su_iir_filt_get(const su_iir_filt_t *filt);

void su_iir_filt_reset(su_iir_filt_t *filt);

/* Initialize filter */
SUBOOL su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    const SUFLOAT *__restrict a,
    SUSCOUNT x_size,
    const SUFLOAT *__restrict b);

/* Initialize filter (internal) */
SUBOOL __su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    SUFLOAT *__restrict a,
    SUSCOUNT x_size,
    SUFLOAT *__restrict b,
    SUBOOL copy_coef);

/* Set output gain */
void su_iir_filt_set_gain(su_iir_filt_t *filt, SUFLOAT gain);

/* Initialize Butterworth low-pass filter of order N */
SUBOOL su_iir_bwlpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc);

/* Initialize Butterworth band-pass filter of order N */
SUBOOL
su_iir_bwbpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT f1, SUFLOAT f2);

/* Initialize Butterworh high-pass filter of order N */
SUBOOL su_iir_bwhpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc);

/* Initialize Root Raised Cosine filter */
SUBOOL
su_iir_rrc_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT T, SUFLOAT beta);

/* Initialize Hilbert transform */
SUBOOL su_iir_hilbert_init(su_iir_filt_t *filt, SUSCOUNT n);

/* Initialize brickwall LPF filter */
SUBOOL su_iir_brickwall_lp_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc);

/* Initialize brickwall BPF filter */
SUBOOL su_iir_brickwall_bp_init(
    su_iir_filt_t *filt,
    SUSCOUNT n,
    SUFLOAT bw,
    SUFLOAT ifnor);

/* Destroy filter */
void su_iir_filt_finalize(su_iir_filt_t *filt);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif  // __clang__
}
#endif /* __cplusplus */

#endif
