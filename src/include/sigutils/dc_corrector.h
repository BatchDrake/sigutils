/*

  Copyright (C) 2023 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_DC_CORRECTOR_H
#define _SIGUTILS_DC_CORRECTOR_H

#include "defs.h"
#include "types.h"


struct sigutils_dc_corrector {
  SUSCOUNT  soft_dc_train_samples;
  SUSCOUNT  soft_dc_count;
  SUBOOL    training;
  SUBOOL    have_dc_offset;
  SUFLOAT   soft_dc_alpha;
  SUCOMPLEX dc_offset;
  SUCOMPLEX dc_c;
};

typedef struct sigutils_dc_corrector su_dc_corrector_t;

#define su_sigutils_dc_corrector_INITIALIZER      \
  {                                               \
    0, 0, SU_FALSE, 0., 0., 0.                    \
  }

SU_METHOD(su_dc_corrector, SUBOOL, init_with_training_period, SUSCOUNT);
SU_METHOD(su_dc_corrector, SUBOOL, init_with_alpha, SUFLOAT);

SU_METHOD(su_dc_corrector, void, set_training_state, SUBOOL);
SU_METHOD(su_dc_corrector, void, reset);
SU_METHOD(su_dc_corrector, void, correct, SUCOMPLEX *buffer, SUSCOUNT size);

#endif /* _SIGUTILS_DC_CORRECTOR_H */
