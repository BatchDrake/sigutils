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

#ifndef _SIGUTILS_NCQO_H
#define _SIGUTILS_NCQO_H

#include "types.h"
#include "sampling.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SU_NCQO_USE_PRECALC_BUFFER
#ifdef SU_NCQO_USE_PRECALC_BUFFER
#  define SU_NCQO_PRECALC_BUFFER_LEN 1024
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

/* The numerically-controlled quadruature oscillator definition */
struct sigutils_ncqo {
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  SUFLOAT phi_buffer[SU_NCQO_PRECALC_BUFFER_LEN];
  SUFLOAT sin_buffer[SU_NCQO_PRECALC_BUFFER_LEN];
  SUFLOAT cos_buffer[SU_NCQO_PRECALC_BUFFER_LEN];
  SUBOOL  pre_c;
  unsigned int p; /* Pointer in precalc buffer */
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  SUFLOAT phi;
  SUFLOAT omega; /* Normalized angular frequency */
  SUFLOAT fnor;  /* Normalized frequency in hcps */

  SUBOOL  sin_updated;
  SUFLOAT sin;

  SUBOOL  cos_updated;
  SUFLOAT cos;
};

typedef struct sigutils_ncqo su_ncqo_t;

#define su_ncqo_INITIALIZER {0, 0, 0, 0, 0}

/* Methods */
SUINLINE SUFLOAT
su_phase_adjust(SUFLOAT phi)
{
  return phi - 2 * PI * SU_FLOOR(phi / (2 * PI));
}

SUINLINE SUFLOAT
su_phase_adjust_one_cycle(SUFLOAT phi)
{
  if (phi > PI)
    return phi - 2 * PI;

  if (phi < - PI)
    return phi + 2 * PI;

  return phi;
}

SUINLINE void
__su_ncqo_step(su_ncqo_t *ncqo)
{
  ncqo->phi += ncqo->omega;

  if (ncqo->phi >= 2 * PI)
    ncqo->phi -= 2 * PI;
  else if (ncqo->phi < 0)
    ncqo->phi += 2 * PI;
}

/* vvvvvvvvvvvvvvvvvvvvvvvvvvv VOLK HACKS BELOW vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
#if defined(_SU_SINGLE_PRECISION) && HAVE_VOLK
#  define SU_USE_VOLK
#  define SU_VOLK_CALL_STRIDE_BITS 5
#  define SU_VOLK_CALL_STRIDE      (1 << SU_VOLK_CALL_STRIDE_BITS)
#  define SU_VOLK_CALL_STRIDE_MASK (SU_VOLK_CALL_STRIDE - 1)
#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#  include <volk/volk.h>
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */
#endif

#ifdef SU_NCQO_USE_PRECALC_BUFFER
SUINLINE void
__su_ncqo_populate_precalc_buffer(su_ncqo_t *ncqo)
{
  unsigned int i;
#ifdef SU_USE_VOLK
  unsigned int p;
#endif /* SU_USE_VOLK */
  /* Precalculate phase buffer */
  for (i = 0; i < SU_NCQO_PRECALC_BUFFER_LEN; ++i) {
    ncqo->phi_buffer[i] = ncqo->phi;
#  ifndef SU_USE_VOLK
#    ifdef HAVE_SINCOS
    SU_SINCOS(ncqo->phi, ncqo->sin_buffer + i, ncqo->cos_buffer + i);
#    else /* HAVE_SINCOS */
    ncqo->sin_buffer[i] = SU_SIN(ncqo->phi);
    ncqo->cos_buffer[i] = SU_COS(ncqo->phi);
#    endif /* HAVE_SINCOS */
#  else
    if ((i & SU_VOLK_CALL_STRIDE_MASK) == SU_VOLK_CALL_STRIDE_MASK) {
      p = i & ~SU_VOLK_CALL_STRIDE_MASK;
      volk_32f_sin_32f(
          ncqo->sin_buffer + p,
          ncqo->phi_buffer + p,
          SU_VOLK_CALL_STRIDE);
      volk_32f_cos_32f(
          ncqo->cos_buffer + p,
          ncqo->phi_buffer + p,
          SU_VOLK_CALL_STRIDE);
    }
#  endif /* SU_USE_VOLK */
    __su_ncqo_step(ncqo);
  }
}

SUINLINE void
__su_ncqo_step_precalc(su_ncqo_t *ncqo)
{
  if (++ncqo->p == SU_NCQO_PRECALC_BUFFER_LEN) {
    ncqo->p = 0;
    __su_ncqo_populate_precalc_buffer(ncqo);
  }
}

#endif /* SU_NCQO_USE_PRECALC_BUFFER */
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ VOLK HACKS ABOVE ^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/* NCQO constructor */
void su_ncqo_init(su_ncqo_t *ncqo, SUFLOAT frel);

/* NCQO constructor for fixed frequency */
void su_ncqo_init_fixed(su_ncqo_t *ncqo, SUFLOAT fnor);

/* Compute next step */
SUINLINE void
su_ncqo_step(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    __su_ncqo_step_precalc(ncqo);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_step(ncqo);

    /* Sine & cosine values are now outdated */
    ncqo->cos_updated = SU_FALSE;
    ncqo->sin_updated = SU_FALSE;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}


/* Force phase */
void su_ncqo_set_phase(su_ncqo_t *ncqo, SUFLOAT phi);

/* Get current phase */
SUINLINE SUFLOAT
su_ncqo_get_phase(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c)
    return ncqo->phi_buffer[ncqo->p];
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return ncqo->phi;
}

/* Increment current phase */
SUINLINE void
su_ncqo_inc_phase(su_ncqo_t *ncqo, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
#  ifdef SU_LOG_DOMAIN
    SU_ERROR("Cannot increase phase on a fixed NCQO\n");
#  endif /* SU_LOG_DOMAIN */
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->phi += delta;

  if (ncqo->phi < 0 || ncqo->phi >= 2 * PI) {
    ncqo->phi -= 2 * PI * SU_FLOOR(ncqo->phi / (2 * PI));
  }
}
/* Get in-phase component */
SUFLOAT su_ncqo_get_i(su_ncqo_t *ncqo);

/* Get cuadrature component */
SUFLOAT su_ncqo_get_q(su_ncqo_t *ncqo);

/* Get both components as complex */
SUCOMPLEX su_ncqo_get(su_ncqo_t *ncqo);

/* Read (compute next + get) in-phase component */
SUFLOAT su_ncqo_read_i(su_ncqo_t *ncqo);

/* Read (compute next + get) cuadrature component */
SUFLOAT su_ncqo_read_q(su_ncqo_t *ncqo);

/* Read (compute next + get) both components as complex */
SUCOMPLEX su_ncqo_read(su_ncqo_t *ncqo);

/* Set oscillator frequency (normalized angular freq) */
void su_ncqo_set_angfreq(su_ncqo_t *ncqo, SUFLOAT omrel);

/* Increase or decrease current frequency (normalized angular freq) */
void su_ncqo_inc_angfreq(su_ncqo_t *ncqo, SUFLOAT delta);

/* Get current frequency (normalized angular freq) */
SUFLOAT su_ncqo_get_angfreq(const su_ncqo_t *ncqo);

/* Set oscillator frequency (normalized freq) */
void su_ncqo_set_freq(su_ncqo_t *ncqo, SUFLOAT frel);

/* Increase or decrease current frequency (normalized freq) */
void su_ncqo_inc_freq(su_ncqo_t *ncqo, SUFLOAT delta);

/* Get current frequency (normalized freq) */
SUFLOAT su_ncqo_get_freq(const su_ncqo_t *ncqo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_NCQO_H */
