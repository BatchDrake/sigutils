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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include <util.h>

#ifdef _SU_SINGLE_PRECISION
#define SUFLOAT    float
#define SU_SOURCE_FFTW_PREFIX fftwf
#else
#define SUFLOAT    double
#define SU_SOURCE_FFTW_PREFIX fftw
#endif

#define SUPRIVATE  static
#define SUINLINE   static inline
#define SUSCOUNT   unsigned long
#define SUSDIFF    long
#define SUBOOL     int
#define SUCOMPLEX  complex SUFLOAT
#define SUSYMBOL   int

#define SU_NOSYMBOL '\0'
#define SU_EOS      -1

#define SUFLOAT_FMT "%lg"
#define SUFLOAT_PRECISION_FMT "%.15lf"

#define SU_FALSE 0
#define SU_TRUE  1

#define SU_COS    cos
#define SU_ACOS   acos
#define SU_SIN    sin
#define SU_ASIN   asin
#define SU_LOG    log10
#define SU_LN     log
#define SU_EXP    exp
#define SU_POW    pow
#define SU_ABS    fabs
#define SU_SQRT   sqrt
#define SU_FLOOR  floor
#define SU_CEIL   ceil
#define SU_COSH   cosh
#define SU_ACOSH  acosh
#define SU_SGN(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

#define SU_C_ABS    cabs
#define SU_C_ARG    carg
#define SU_C_REAL   creal
#define SU_C_IMAG   cimag
#define SU_C_EXP    cexp
#define SU_C_CONJ   conj
#define SU_C_SGN(x) (SU_SGN(SU_C_REAL(x)) + I * SU_SGN(SU_C_IMAG(x)))

#define SUFLOAT_MIN_REF_MAG 1e-8
#define SUFLOAT_MIN_REF_DB  -160
#define SUFLOAT_THRESHOLD 1e-15
#define SUFLOAT_MAX_REF_MAG 1
#define SUFLOAT_MAX_REF_DB  0

#define SUFLOAT_EQUAL(a, b) (SU_ABS(a - b) <= SUFLOAT_THRESHOLD)
#define SU_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SU_MIN(a, b) ((a) < (b) ? (a) : (b))

#define SU_DB_RAW(p) (20 * SU_LOG(p))
#define SU_DB(p) SU_DB_RAW((p) + SUFLOAT_MIN_REF_MAG)

#define SU_MAG_RAW(d) SU_POW(10.0, (d) / 20.)
#define SU_MAG(d) (SU_MAG_RAW(d) - SUFLOAT_MIN_REF_MAG)

/* Required for the map from sigutils types to FFTW3 types */
#define SU_FFTW(method) JOIN(SU_SOURCE_FFTW_PREFIX, method)

#define SU_ERROR(fmt, arg...) \
  fprintf(stderr, "(e) %s:%d: " fmt, __FUNCTION__, __LINE__, ##arg)
#define SU_WARNING(fmt, arg...) \
  fprintf(stderr, "(!) %s:%d: " fmt, __FUNCTION__, __LINE__, ##arg)
#define SU_INFO(fmt, arg...) \
  fprintf(stderr, "(i) %s:%d: " fmt, __FUNCTION__, __LINE__, ##arg)

/* Inline functions */

/* Normalized sinc */
SUINLINE SUFLOAT
su_sinc(SUFLOAT t)
{
  /* Use l'Hôpital's near 0 */
  if (SU_ABS(t) <= SUFLOAT_THRESHOLD)
    return SU_COS(M_PI * t);

  return SU_SIN(M_PI * t) / (M_PI * t);
}

/* Additive white gaussian noise (AWGN) generator, Box-Muller method */
SUINLINE SUCOMPLEX
su_c_awgn(void)
{
  SUFLOAT U1 = ((SUFLOAT) rand() + 1.) / ((SUFLOAT) RAND_MAX + 1.);
  SUFLOAT U2 = ((SUFLOAT) rand() + 1.) / ((SUFLOAT) RAND_MAX + 1.);
  SUFLOAT SQ = SU_SQRT(-2 * SU_LN(U1));
  SUFLOAT PH = 2 * M_PI * U2;

  return  SQ * (SU_COS(PH) + I * SU_SIN(PH));
}

#endif /* _SIGUTILS_TYPES_H */
