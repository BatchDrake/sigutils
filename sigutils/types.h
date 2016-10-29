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

#ifndef _SIGUTILS_TYPES_H
#define _SIGUTILS_TYPES_H

#include <math.h>
#include <complex.h>

#define SUPRIVATE  static inline
#define SUFLOAT    double
#define SUSCOUNT   unsigned long
#define SUBOOL     int
#define SUCOMPLEX  complex SUFLOAT

#define SUFLOAT_FMT "%lg"
#define SUFLOAT_PRECISION_FMT "%.15lf"

#define SU_FALSE 0
#define SU_TRUE  1

#define SU_COS cos
#define SU_SIN sin
#define SU_LOG log10
#define SU_EXP exp
#define SU_POW pow
#define SU_ABS fabs

#define SUFLOAT_MIN_REF_MAG 1e-8
#define SUFLOAT_MIN_REF_DB  -160
#define SUFLOAT_THRESHOLD 1e-15
#define SUFLOAT_MAX_REF_MAG 1
#define SUFLOAT_MAX_REF_DB  0

#define SUFLOAT_EQUAL(a, b) (fabs(a - b) <= SUFLOAT_THRESHOLD)
#define SU_MAX(a, b) ((a) > (b) ? (a) : (b))

#define SU_DB_RAW(p) (20 * SU_LOG(p))
#define SU_DB(p) SU_DB_RAW((p) + SUFLOAT_MIN_REF_MAG)

#define SU_MAG_RAW(d) SU_POW(10.0, (d) / 20.)
#define SU_MAG(d) (SU_MAG_RAW(d) - SUFLOAT_MIN_REF_MAG)

#endif /* _SIGUTILS_TYPES_H */
