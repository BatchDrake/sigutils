/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016-2023 Gonzalo José Carracedo Carballal
 * Copyright (C) 2023      Antonio Vázquez Blanco
 */

/**
 * \brief Numerically-controlled quadrature oscillator.
 *
 * This file exposes the API to instaniate and interact with the
 * implementation of a numerically-controlled quadrature oscillator.
 */

#ifndef _SIGUTILS_NCQO_H
#define _SIGUTILS_NCQO_H

#include <sigutils/defs.h>
#include <sigutils/log.h>
#include <sigutils/sampling.h>
#include <sigutils/types.h>

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif  // __clang__
extern "C" {
#endif /* __cplusplus */

#define SU_NCQO_USE_PRECALC_BUFFER  /**< Use a precalculated buffer for the oscillator. */
#ifdef SU_NCQO_USE_PRECALC_BUFFER
#  define SU_NCQO_PRECALC_BUFFER_LEN 1024
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

/**
 * This structure holds the internal state of an instance of a
 * numerically-controlled quadrature oscillator.
 */
struct sigutils_ncqo {
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  SUFLOAT phi_buffer[SU_NCQO_PRECALC_BUFFER_LEN]; /**< Precalculated angular positions of the oscillator */
  SUFLOAT sin_buffer[SU_NCQO_PRECALC_BUFFER_LEN]; /**< Precalculated sine values of the oscillator */
  SUFLOAT cos_buffer[SU_NCQO_PRECALC_BUFFER_LEN]; /**< Precalculated cosine values of the oscillator */
  SUBOOL pre_c;   /**< Current instance is using precalculated values */
  unsigned int p; /**< Current index in use in the precalcalculated buffers */
#endif            /* SU_NCQO_USE_PRECALC_BUFFER */

  SUFLOAT phi;   /**< Current angular position of the oscillator */
  SUFLOAT omega; /**< Normalized angular frequency */
  SUFLOAT fnor;  /**< Normalized frequency in hcps */

  SUBOOL sin_updated; /**< Flag to indicate that sine value is updated */
  SUFLOAT sin;        /**< Sine value for the current angular position of the oscillator */

  SUBOOL cos_updated; /**< Flag to indicate that cosine value is updated */
  SUFLOAT cos;        /**< Cosine value for the current angular position of the oscillator */
};

typedef struct sigutils_ncqo su_ncqo_t;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
#  define su_ncqo_INITIALIZER                                               \
    {                                                                       \
      {0.}, {0.}, {0.}, SU_FALSE, 0, 0., 0., 0., SU_FALSE, 0., SU_FALSE, 0. \
    }
#else
#  define su_ncqo_INITIALIZER                \
    {                                        \
      0., 0., 0., SU_FALSE, 0., SU_FALSE, 0. \
    }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

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

  if (phi < -PI)
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
#  define SU_VOLK_CALL_STRIDE (1 << SU_VOLK_CALL_STRIDE_BITS)
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
#  ifdef SU_USE_VOLK
  unsigned int p;
#  endif /* SU_USE_VOLK */
  /* Precalculate phase buffer */
  for (i = 0; i < SU_NCQO_PRECALC_BUFFER_LEN; ++i) {
    ncqo->phi_buffer[i] = ncqo->phi;
#  ifndef SU_USE_VOLK
#    ifdef HAVE_SINCOS
    SU_SINCOS(ncqo->phi, ncqo->sin_buffer + i, ncqo->cos_buffer + i);
#    else  /* HAVE_SINCOS */
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
SU_CONSTRUCTOR_TYPED(void, su_ncqo, SUFLOAT frel);

/* NCQO constructor for fixed frequency */
SU_METHOD(su_ncqo, void, init_fixed, SUFLOAT fnor);

/* Init ncqo from existing ncqo */
SU_METHOD(su_ncqo, void, copy, const su_ncqo_t *ncqo);

/* Compute next step */
SUINLINE
SU_METHOD(su_ncqo, void, step)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    __su_ncqo_step_precalc(self);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_step(self);

    /* Sine & cosine values are now outdated */
    self->cos_updated = SU_FALSE;
    self->sin_updated = SU_FALSE;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

/* Force phase */
SU_METHOD(su_ncqo, void, set_phase, SUFLOAT phi);

/* Get current phase */
SUINLINE
SU_GETTER(su_ncqo, SUFLOAT, get_phase)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c)
    return self->phi_buffer[self->p];
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return self->phi;
}

/* Increment current phase */
SUINLINE
SU_METHOD(su_ncqo, void, inc_phase, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
#  ifdef SU_LOG_DOMAIN
    SU_ERROR("Cannot increase phase on a fixed NCQO\n");
#  endif /* SU_LOG_DOMAIN */
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->phi += delta;

  if (self->phi < 0 || self->phi >= 2 * PI) {
    self->phi -= 2 * PI * SU_FLOOR(self->phi / (2 * PI));
  }
}
/* Get in-phase component */
SU_METHOD(su_ncqo, SUFLOAT, get_i);

/* Get cuadrature component */
SU_METHOD(su_ncqo, SUFLOAT, get_q);

/* Get both components as complex */
SU_METHOD(su_ncqo, SUCOMPLEX, get);

/* Read (compute next + get) in-phase component */
SU_METHOD(su_ncqo, SUFLOAT, read_i);

/* Read (compute next + get) cuadrature component */
SU_METHOD(su_ncqo, SUFLOAT, read_q);

/* Read (compute next + get) both components as complex */
SU_METHOD(su_ncqo, SUCOMPLEX, read);

/* Set oscillator frequency (normalized angular freq) */
SU_METHOD(su_ncqo, void, set_angfreq, SUFLOAT omrel);

/* Increase or decrease current frequency (normalized angular freq) */
SU_METHOD(su_ncqo, void, inc_angfreq, SUFLOAT delta);

/* Get current frequency (normalized angular freq) */
SU_GETTER(su_ncqo, SUFLOAT, get_angfreq);

/* Set oscillator frequency (normalized freq) */
SU_METHOD(su_ncqo, void, set_freq, SUFLOAT frel);

/* Increase or decrease current frequency (normalized freq) */
SU_METHOD(su_ncqo, void, inc_freq, SUFLOAT delta);

/* Get current frequency (normalized freq) */
SU_GETTER(su_ncqo, SUFLOAT, get_freq);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif  // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_NCQO_H */
