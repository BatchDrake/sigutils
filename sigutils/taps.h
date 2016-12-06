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

#ifndef _SIGUTILS_TAPS_H
#define _SIGUTILS_TAPS_H

#define SU_HAMMING_ALPHA 0.54
#define SU_MAMMING_BETA (1 - SU_HAMMING_ALPHA)

#define SU_HANN_ALPHA 0.5
#define SU_HANN_BETA (1 - SU_HANN_ALPHA)

/* Get coefficients of a RRC filter */
void su_taps_rrc_init(SUFLOAT *h, SUFLOAT T, SUFLOAT beta, SUSCOUNT size);

void su_taps_brickwall_lp_init(SUFLOAT *h, SUFLOAT fc, SUSCOUNT size);

void su_taps_brickwall_bp_init(SUFLOAT *h, SUFLOAT bw, SUFLOAT if_nor, SUSCOUNT size);

#endif /* _SIGUTILS_TAPS_H */
