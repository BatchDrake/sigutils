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

#ifndef _SIGUTILS_TAPS_H
#define _SIGUTILS_TAPS_H

#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SU_HAMMING_ALPHA 0.54
#define SU_MAMMING_BETA (1 - SU_HAMMING_ALPHA)

#define SU_HANN_ALPHA 0.5
#define SU_HANN_BETA (1 - SU_HANN_ALPHA)

#define SU_FLAT_TOP_A0 1
#define SU_FLAT_TOP_A1 1.93
#define SU_FLAT_TOP_A2 1.29
#define SU_FLAT_TOP_A3 0.388
#define SU_FLAT_TOP_A4 0.028

#define SU_BLACKMANN_HARRIS_A0 0.35875
#define SU_BLACKMANN_HARRIS_A1 0.48829
#define SU_BLACKMANN_HARRIS_A2 0.14128
#define SU_BLACKMANN_HARRIS_A3 0.01168

/* Window functions */
void su_taps_apply_hamming(SUFLOAT *h, SUSCOUNT size);
void su_taps_apply_hann(SUFLOAT *h, SUSCOUNT size);

void su_taps_apply_hamming_complex(SUCOMPLEX *h, SUSCOUNT size);
void su_taps_apply_hann_complex(SUCOMPLEX *h, SUSCOUNT size);

void su_taps_apply_flat_top(SUFLOAT *h, SUSCOUNT size);
void su_taps_apply_flat_top_complex(SUCOMPLEX *h, SUSCOUNT size);

void su_taps_apply_blackmann_harris(SUFLOAT *h, SUSCOUNT size);
void su_taps_apply_blackmann_harris_complex(SUCOMPLEX *h, SUSCOUNT size);

/* Hilbert transform */
void su_taps_hilbert_init(SUFLOAT *h, SUSCOUNT size);

/* Get coefficients of a RRC filter */
void su_taps_rrc_init(SUFLOAT *h, SUFLOAT T, SUFLOAT beta, SUSCOUNT size);

void su_taps_brickwall_lp_init(SUFLOAT *h, SUFLOAT fc, SUSCOUNT size);

void su_taps_brickwall_bp_init(
    SUFLOAT *h,
    SUFLOAT bw,
    SUFLOAT if_nor,
    SUSCOUNT size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_TAPS_H */
