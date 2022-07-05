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

#include <sigutils/agc.h>
#include <sigutils/iir.h>
#include <sigutils/ncqo.h>
#include <sigutils/pll.h>
#include <sigutils/sampling.h>
#include <sigutils/sigutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_list.h"
#include "test_param.h"

SUPRIVATE SUFLOAT
su_test_cdr_block_symbol_uncertainty(SUCOMPLEX symbol)
{
  SUCOMPLEX symbols[] = {1 + I, 1 - I, -1 + I, -1 - I};
  unsigned int i = 0;
  SUFLOAT dist = INFINITY;

  for (i = 0; i < sizeof(symbols) / sizeof(SUCOMPLEX); ++i)
    if (SU_C_ABS(symbol - symbols[i]) < dist)
      dist = SU_C_ABS(symbol - symbols[i]);

  return dist;
}
