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
#  define SUFLOAT    float
#  define SU_SOURCE_FFTW_PREFIX fftwf
#else
#  define SUFLOAT    double
#  define SU_SOURCE_FFTW_PREFIX fftw
#endif

#define SUPRIVATE  static
#define SUINLINE   static inline
#define SUSCOUNT   unsigned long
#define SUSDIFF    long
#define SUBOOL     int
#define SUCOMPLEX  complex SUFLOAT
#define SUSYMBOL   int
#define SUBITS     unsigned char /* Not exactly a bit */

#define SU_NOSYMBOL '\0'
#define SU_EOS      -1

#ifdef _SU_SINGLE_PRECISION
#  define SUFLOAT_FMT "%g"
#  define SUFLOAT_PRECISION_FMT "%.15f"
#  define SUFLOAT_SCANF_FMT "%f"
#  define SU_ADDSFX(token) JOIN(token, f)
#else
#  define SUFLOAT_FMT "%lg"
#  define SUFLOAT_PRECISION_FMT "%.15lf"
#  define SUFLOAT_SCANF_FMT "%lf"
#  define SU_ADDSFX(token) token
#endif

#define SU_FALSE 0
#define SU_TRUE  1


#define SU_SQRT2  1.41421356237
#define SU_COS    SU_ADDSFX(cos)
#define SU_ACOS   SU_ADDSFX(acos)
#define SU_SIN    SU_ADDSFX(sin)
#define SU_ASIN   SU_ADDSFX(asin)
#define SU_TAN    SU_ADDSFX(tan)
#define SU_LOG    SU_ADDSFX(log10)
#define SU_LN     SU_ADDSFX(log)
#define SU_EXP    SU_ADDSFX(exp)
#define SU_POW    SU_ADDSFX(pow)
#define SU_ABS    SU_ADDSFX(fabs)
#define SU_SQRT   SU_ADDSFX(sqrt)
#define SU_FLOOR  SU_ADDSFX(floor)
#define SU_CEIL   SU_ADDSFX(ceil)
#define SU_ROUND  SU_ADDSFX(round)
#define SU_COSH   SU_ADDSFX(cosh)
#define SU_ACOSH  SU_ADDSFX(acosh)
#define SU_SINCOS SU_ADDSFX(sincos) /* May be unavailable, see config.h */

#define SU_SGN(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

#define SU_C_ABS    SU_ADDSFX(cabs)
#define SU_C_ARG    SU_ADDSFX(carg)
#define SU_C_REAL   SU_ADDSFX(creal)
#define SU_C_IMAG   SU_ADDSFX(cimag)
#define SU_C_EXP    SU_ADDSFX(cexp)
#define SU_C_CONJ   SU_ADDSFX(conj)
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

#define SU_POWER_DB_RAW(p) (10 * SU_LOG(p))
#define SU_POWER_DB(p) SU_POWER_DB_RAW((p) + SUFLOAT_MIN_REF_MAG)

#define SU_MAG_RAW(d) SU_POW(10.0, (d) / 20.)
#define SU_MAG(d) (SU_MAG_RAW(d) - SUFLOAT_MIN_REF_MAG)

#define SU_POWER_MAG_RAW(d) SU_POW(10.0, (d) / 10.)
#define SU_POWER_MAG(d) (SU_POWER_MAG_RAW(d) - SUFLOAT_MIN_REF_MAG)

/* Required for the map from sigutils types to FFTW3 types */
#define SU_FFTW(method) JOIN(SU_SOURCE_FFTW_PREFIX, method)

/* Symbol manipulation */
#define SU_FROMSYM(x) ((x) - '0')
#define SU_TOSYM(x)   ((x) + '0')
#define SU_ISSYM(x)   ((x) >= '0')

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
  SUFLOAT SQ = SU_SQRT(-SU_LN(U1));
  SUFLOAT PH = 2 * M_PI * U2;

  return  SQ * (SU_COS(PH) + I * SU_SIN(PH));
}

#endif /* _SIGUTILS_TYPES_H */
