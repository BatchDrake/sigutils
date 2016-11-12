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

#include <string.h>
#include "sampling.h"
#include "types.h"
#include "pll.h"

void
su_pll_finalize(su_pll_t *pll)
{

}

SUBOOL
su_pll_init(su_pll_t *pll, SUFLOAT fhint, SUFLOAT fc)
{
  memset(pll, 0, sizeof(su_pll_t));

  pll->alpha = SU_NORM2ANG_FREQ(fc);
  pll->beta  = SU_SQRT(pll->alpha);

  su_ncqo_init(&pll->ncqo, fhint);

  return SU_TRUE;

fail:
  su_pll_finalize(pll);

  return SU_FALSE;
}


void
su_pll_feed(su_pll_t *pll, SUFLOAT x)
{
  SUCOMPLEX s;

  SUFLOAT lck = 0;
  SUFLOAT err = 0;

  s = su_ncqo_read(&pll->ncqo);

  err = -x * cimag(s); /* Error signal: projection against Q */
  lck =  x * creal(s); /* Lock: projection against I */

  pll->lock += pll->beta * (2 * lck - pll->lock);

  if (pll->ncqo.omega > -pll->alpha * err) {
    su_ncqo_inc_angfreq(&pll->ncqo, pll->alpha * err);
  }

  su_ncqo_inc_phase(&pll->ncqo, pll->beta * err);
}

