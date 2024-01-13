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

#ifndef _SIGUTILS_TYPES_H
#define _SIGUTILS_TYPES_H

#if defined(__GNUC__) && !defined(_GNU_SOURCE)
#  define _GNU_SOURCE
#endif /* __GNUC__ */

#include <complex.h>
#include <fftw3.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sigutils/util/util.h>

#if defined(__cplusplus)
#  define SU_USE_CPP_COMPLEX_API
#endif

#if __STDC_VERSION__ >= 199901L
#  define SU_ENABLE_C99
#endif /* __STDC_VERSION__ */

#if defined(SU_ENABLE_C99)
#  define __restrict restrict
#elif !defined(__GNUC__)
#  define __restrict
#endif

#ifdef SU_USE_CPP_COMPLEX_API
#  ifdef I
#    undef I
#  endif /* I */
#  define SU_I            \
    std::complex<SUFLOAT> \
    {                     \
      0, 1                \
    }
#  define SUCOMPLEX std::complex<SUFLOAT>
#  define SU_C_REAL(c) (c).real()
#  define SU_C_IMAG(c) (c).imag()
#  define SU_C_ABS(c) std::abs(c)
#  define SU_C_ARG(c) std::arg(c)
#  define SU_C_EXP(c) std::exp(c)
#  define SU_C_CONJ(c) std::conj(c)
#  define SU_C_SGN(x) SUCOMPLEX(SU_SGN(SU_C_REAL(x)), SU_SGN(SU_C_IMAG(x)))
#else
#  define SU_I I
#  define SUCOMPLEX _Complex SUFLOAT
#  define SU_C_REAL(c) (SU_ADDSFX(creal)(c))
#  define SU_C_IMAG(c) (SU_ADDSFX(cimag)(c))
#  define SU_C_ABS SU_ADDSFX(cabs)
#  define SU_C_ARG SU_ADDSFX(carg)
#  define SU_C_EXP SU_ADDSFX(cexp)
#  define SU_C_CONJ SU_ADDSFX(conj)
#  define SU_C_SGN(x) (SU_SGN(SU_C_REAL(x)) + I * SU_SGN(SU_C_IMAG(x)))
#endif

#define SUSINGLE float
#define SUDOUBLE double

#ifdef _SU_SINGLE_PRECISION
#  define SUFLOAT SUSINGLE
#  define SU_SOURCE_FFTW_PREFIX fftwf
#else
#  define SUFLOAT SUDOUBLE
#  define SU_SOURCE_FFTW_PREFIX fftw
#endif

#define SUPRIVATE static
#define SUSCOUNT uint64_t
#define SUSDIFF int64_t
#define SUBOOL int
#define SUFREQ double
#define SUSYMBOL int
#define SUBITS uint8_t /* Not exactly a bit */

#ifdef __cplusplus
#  define SUINLINE inline
#else
#  define SUINLINE static inline
#endif /* __cplusplus */

/* Perform casts in C and C++ */
#ifdef __cplusplus
#  define SUCAST(type, value) static_cast<type>(value)
#else
#  define SUCAST(type, value) ((type)(value))
#endif /* __cplusplus */

#define SU_ASFLOAT(value) SUCAST(SUFLOAT, value)
#define SUIMM(imm) SU_ASFLOAT(imm)

#define SU_NOSYMBOL '\0'
#define SU_EOS -1

#define SUSINGLE_FMT "%g"
#define SUSINGLE_SCANF_FMT "%f"
#define SUSINGLE_PRECISION_FMT "%.8f"

#define SUDOUBLE_FMT "%lg"
#define SUDOUBLE_SCANF_FMT "%lf"
#define SUDOUBLE_PRECISION_FMT "%.16lf"

#ifdef _SU_SINGLE_PRECISION
#  define SUFLOAT_FMT SUSINGLE_FMT
#  define SUFLOAT_PRECISION_FMT SUSINGLE_PRECISION_FMT
#  define SUFLOAT_SCANF_FMT SUSINGLE_SCANF_FMT
#  define SU_ADDSFX(token) JOIN(token, f)
#else
#  define SUFLOAT_FMT SUDOUBLE_FMT
#  define SUFLOAT_PRECISION_FMT SUDOUBLE_PRECISION_FMT
#  define SUFLOAT_SCANF_FMT SUDOUBLE_SCANF_FMT
#  define SU_ADDSFX(token) token
#endif

#define SU_FALSE 0
#define SU_TRUE 1

#define SU_SQRT2 1.41421356237
#define SU_COS SU_ADDSFX(cos)
#define SU_ACOS SU_ADDSFX(acos)
#define SU_SIN SU_ADDSFX(sin)
#define SU_ASIN SU_ADDSFX(asin)
#define SU_TAN SU_ADDSFX(tan)
#define SU_LOG SU_ADDSFX(log10)
#define SU_LN SU_ADDSFX(log)
#define SU_EXP SU_ADDSFX(exp)
#define SU_POW SU_ADDSFX(pow)
#define SU_ABS SU_ADDSFX(fabs)
#define SU_SQRT SU_ADDSFX(sqrt)
#define SU_FLOOR SU_ADDSFX(floor)
#define SU_CEIL SU_ADDSFX(ceil)
#define SU_ROUND SU_ADDSFX(round)
#define SU_COSH SU_ADDSFX(cosh)
#define SU_ACOSH SU_ADDSFX(acosh)
#define SU_FMOD SU_ADDSFX(fmod)
#define SU_ATAN2 SU_ADDSFX(atan2)
#define SU_MODF SU_ADDSFX(modf)

/* Extended functions with extra precision */
#define SU_SQRTX(x) SU_ASFLOAT(sqrt(x))
#define SU_COSX(x) SU_ASFLOAT(cos(x))
#define SU_SINX(x) SU_ASFLOAT(sin(x))

#define SU_POWX(x, y) pow(x, y)

#ifdef __APPLE__
#  define SU_SINCOS SU_ADDSFX(__sincos)
#elif defined(__GNUC__)
#  define SU_SINCOS SU_ADDSFX(sincos)
#else
#  define SU_SINCOS(phi, sinptr, cosptr) \
    do {                                 \
      *(sinptr) = SU_SIN(phi);           \
      *(cosptr) = SU_COS(phi);           \
    } while (0)
#endif /* __APPLE__ */

#define SU_SPLPF_ALPHA(tau) (1.f - SU_EXP(-1.f / (tau)))
#define SU_SPLPF_FEED(y, x, alpha) y += (alpha) * ((x) - (y))
#define SU_VALID isfinite
#define SU_C_VALID(x) (SU_VALID(SU_C_REAL(x)) && SU_VALID(SU_C_IMAG(x)))
#define SU_SGN(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))
#define SU_MOD(x, d) SU_FMOD(x, d)
#define SU_SQR(x) ((x) * (x))
#define SU_CUBE(x) (SU_SQR(x) * (x))

#define SU_RAD2DEG(rad) ((rad) * (180 / PI))
#define SU_DEG2RAD(rad) ((rad) * (PI / 180))

#ifndef PI
#  define PI SU_ADDSFX(3.141592653589793238462643)
#endif

#ifndef M_PI
#  define M_PI PI
#endif

#ifndef INFINITY
#  define INFINITY (1.0 / 0)
#endif

#define sufcmp(a, b, tol) (SU_ABS(SU_ASFLOAT(a) - SU_ASFLOAT(b)) > (tol))
#define sufrelcmp(a, b, tol) sufcmp(1, SU_ASFLOAT(a) / SU_ASFLOAT(b), tol)

#define sufeq(a, b, tol) (sufcmp(a, b, tol) == 0)
#define sufreleq(a, b, tol) (sufrelcmp(a, b, tol) == 0)

#define SUFLOAT_THRESHOLD SU_ADDSFX(1e-15)
#define SUFLOAT_MIN_REF_MAG SUFLOAT_THRESHOLD
#define SUFLOAT_MIN_REF_DB -300
#define SUFLOAT_MAX_REF_MAG 1
#define SUFLOAT_MAX_REF_DB 0

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
#define SU_TOSYM(x) ((x) + '0')
#define SU_ISSYM(x) ((x) >= '0')

/* Inline functions */

/* Normalized sinc */
SUINLINE SUFLOAT
su_sinc(SUFLOAT t)
{
  /* Use l'Hôpital's near 0 */
  if (SU_ABS(t) <= SUFLOAT_THRESHOLD)
    return SU_COS(PI * t);

  return SU_SIN(PI * t) / (PI * t);
}

/* Additive white gaussian noise (AWGN) generator, Box-Muller method */
SUINLINE SUCOMPLEX
su_c_awgn(void)
{
  SUFLOAT U1 = SU_ASFLOAT(rand() + 1.) / SU_ASFLOAT(RAND_MAX + 1.);
  SUFLOAT U2 = SU_ASFLOAT(rand() + 1.) / SU_ASFLOAT(RAND_MAX + 1.);
  SUFLOAT SQ = SU_SQRT(-SU_LN(U1));
  SUFLOAT PH = 2 * PI * U2;

  return SQ * (SU_COS(PH) + SU_I * SU_SIN(PH));
}

#endif /* _SIGUTILS_TYPES_H */
