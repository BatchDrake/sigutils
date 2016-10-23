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

#define SUFLOAT_THRESHOLD 1e-15

#define SUFLOAT_EQUAL(a, b) (fabs(a - b) <= SUFLOAT_THRESHOLD)
#define SU_MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* _SIGUTILS_TYPES_H */
