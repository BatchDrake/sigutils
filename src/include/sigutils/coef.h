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

#ifndef _SIGUTILS_COEF_H
#define _SIGUTILS_COEF_H

#include <sigutils/types.h>

SUFLOAT *su_dcof_bwlp(int n, SUFLOAT fcf);
SUFLOAT *su_dcof_bwhp(int n, SUFLOAT fcf);
SUFLOAT *su_dcof_bwbp(int n, SUFLOAT f1f, SUFLOAT f2f);
SUFLOAT *su_dcof_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f);

SUFLOAT *su_ccof_bwlp(int n);
SUFLOAT *su_ccof_bwhp(int n);
SUFLOAT *su_ccof_bwbp(int n);
SUFLOAT *su_ccof_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f);

SUFLOAT su_sf_bwlp(int n, SUFLOAT fcf);
SUFLOAT su_sf_bwhp(int n, SUFLOAT fcf);
SUFLOAT su_sf_bwbp(int n, SUFLOAT f1f, SUFLOAT f2f);
SUFLOAT su_sf_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f);

#endif /* _SIGUTILS_COEF_H */
