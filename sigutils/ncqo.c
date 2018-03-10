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

#define _GNU_SOURCE
#include <math.h>

#define SU_LOG_DOMAIN "ncqo"

#include "ncqo.h"
#include "sampling.h"
#include "config.h"
#include "log.h"

#if defined(_SU_SINGLE_PRECISION) && HAVE_VOLK
#  define SU_USE_VOLK
#  define SU_VOLK_CALL_STRIDE_BITS 5
#  define SU_VOLK_CALL_STRIDE      (1 << SU_VOLK_CALL_STRIDE_BITS)
#  define SU_VOLK_CALL_STRIDE_MASK (SU_VOLK_CALL_STRIDE - 1)
#  include <volk/volk.h>
#endif

SUINLINE void
__su_ncqo_step(su_ncqo_t *ncqo)
{
  ncqo->phi += ncqo->omega;

  if (ncqo->phi >= 2 * PI)
    ncqo->phi -= 2 * PI;
  else if (ncqo->phi < 0)
    ncqo->phi += 2 * PI;
}

#ifdef SU_NCQO_USE_PRECALC_BUFFER
SUINLINE void
__su_ncqo_populate_precalc_buffer(su_ncqo_t *ncqo)
{
  unsigned int i, p;

  /* Precalculate phase buffer */
  for (i = 0; i < SU_NCQO_PRECALC_BUFFER_LEN; ++i) {
    ncqo->phi_buffer[i] = ncqo->phi;
#  ifndef SU_USE_VOLK
#    ifdef HAVE_SINCOS
    SU_SINCOS(ncqo->phi, ncqo->sin_buffer + i, ncqo->cos_buffer + i);
#    else
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

/* Expects: relative frequency */
void
su_ncqo_init(su_ncqo_t *ncqo, SUFLOAT fnor)
{
  ncqo->phi   = .0;
  ncqo->omega = SU_NORM2ANG_FREQ(fnor);
  ncqo->fnor  = fnor;
  ncqo->sin   = 0;
  ncqo->cos   = 1;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  ncqo->p     = 0;
  ncqo->pre_c = SU_FALSE;
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

void
su_ncqo_init_fixed(su_ncqo_t *ncqo, SUFLOAT fnor)
{
  su_ncqo_init(ncqo, fnor);

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  ncqo->pre_c = SU_TRUE;
  __su_ncqo_populate_precalc_buffer(ncqo);
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SUINLINE void
__su_ncqo_assert_cos(su_ncqo_t *ncqo)
{
  if (!ncqo->cos_updated) {
    ncqo->cos = SU_COS(ncqo->phi);
    ++ncqo->cos_updated;
  }
}

SUINLINE void
__su_ncqo_assert_sin(su_ncqo_t *ncqo)
{
  if (!ncqo->sin_updated) {
    ncqo->sin = SU_SIN(ncqo->phi);
    ++ncqo->sin_updated;
  }
}

void
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

void
su_ncqo_set_phase(su_ncqo_t *ncqo, SUFLOAT phi)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot set phase on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->phi = phi - 2 * PI * SU_FLOOR(phi / (2 * PI));
}

SUFLOAT
su_ncqo_get_phase(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c)
    return ncqo->phi_buffer[ncqo->p];
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return ncqo->phi;
}

void
su_ncqo_inc_phase(su_ncqo_t *ncqo, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot increase phase on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->phi += delta;

  if (ncqo->phi < 0 || ncqo->phi >= 2 * PI) {
    ncqo->phi -= 2 * PI * SU_FLOOR(ncqo->phi / (2 * PI));
  }
}

SUFLOAT
su_ncqo_get_i(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    return ncqo->cos_buffer[ncqo->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_assert_cos(ncqo);
    return ncqo->cos;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SUFLOAT
su_ncqo_get_q(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    return ncqo->sin_buffer[ncqo->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_assert_sin(ncqo);
    return ncqo->sin;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SUCOMPLEX
su_ncqo_get(su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    return ncqo->cos_buffer[ncqo->p] + I * ncqo->sin_buffer[ncqo->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
  __su_ncqo_assert_cos(ncqo);
  __su_ncqo_assert_sin(ncqo);

  return ncqo->cos + ncqo->sin * I;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SUFLOAT
su_ncqo_read_i(su_ncqo_t *ncqo)
{
  SUFLOAT old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    old = ncqo->cos_buffer[ncqo->p];
    __su_ncqo_step_precalc(ncqo);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = ncqo->cos;

    __su_ncqo_step(ncqo);

    ncqo->cos_updated = SU_TRUE;
    ncqo->sin_updated = SU_FALSE;
    ncqo->cos = SU_COS(ncqo->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

SUFLOAT
su_ncqo_read_q(su_ncqo_t *ncqo)
{
  SUFLOAT old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    old = ncqo->sin_buffer[ncqo->p];
    __su_ncqo_step_precalc(ncqo);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = ncqo->sin;

    __su_ncqo_step(ncqo);

    ncqo->cos_updated = SU_FALSE;
    ncqo->sin_updated = SU_TRUE;
    ncqo->sin = SU_SIN(ncqo->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

SUCOMPLEX
su_ncqo_read(su_ncqo_t *ncqo)
{
  SUCOMPLEX old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    old = ncqo->cos_buffer[ncqo->p] + I * ncqo->sin_buffer[ncqo->p];
    __su_ncqo_step_precalc(ncqo);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = ncqo->cos + I * ncqo->sin;

    __su_ncqo_step(ncqo);

    ncqo->cos_updated = SU_TRUE;
    ncqo->sin_updated = SU_TRUE;

    ncqo->cos = SU_COS(ncqo->phi);
    ncqo->sin = SU_SIN(ncqo->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

void
su_ncqo_set_angfreq(su_ncqo_t *ncqo, SUFLOAT omrel)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot change frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->omega = omrel;
  ncqo->fnor  = SU_ANG2NORM_FREQ(omrel);
}

void
su_ncqo_inc_angfreq(su_ncqo_t *ncqo, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot increase frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->omega += delta;
  ncqo->fnor   = SU_ANG2NORM_FREQ(ncqo->omega);
}

SUFLOAT
su_ncqo_get_angfreq(const su_ncqo_t *ncqo)
{
  return ncqo->omega;
}

void
su_ncqo_set_freq(su_ncqo_t *ncqo, SUFLOAT fnor)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot change frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->fnor  = fnor;
  ncqo->omega = SU_NORM2ANG_FREQ(fnor);
}

void
su_ncqo_inc_freq(su_ncqo_t *ncqo, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (ncqo->pre_c) {
    SU_ERROR("Cannot increase frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  ncqo->fnor  += delta;
  ncqo->omega  = SU_NORM2ANG_FREQ(ncqo->fnor);
}

SUFLOAT
su_ncqo_get_freq(const su_ncqo_t *ncqo)
{
  return ncqo->fnor;
}

