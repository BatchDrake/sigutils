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

#define _GNU_SOURCE
#include <math.h>
#include <string.h>

#define SU_LOG_DOMAIN "ncqo"

#include <sigutils/log.h>
#include <sigutils/ncqo.h>
#include <sigutils/sampling.h>

/* Expects: relative frequency */
SU_CONSTRUCTOR_TYPED(void, su_ncqo, SUFLOAT fnor)
{
  self->phi = .0;
  self->omega = SU_NORM2ANG_FREQ(fnor);
  self->fnor = fnor;
  self->sin = 0;
  self->cos = 1;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  self->p = 0;
  self->pre_c = SU_FALSE;
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SU_METHOD(su_ncqo, void, init_fixed, SUFLOAT fnor)
{
  su_ncqo_init(self, fnor);

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  self->pre_c = SU_TRUE;
  __su_ncqo_populate_precalc_buffer(self);
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SU_METHOD(su_ncqo, void, copy, const su_ncqo_t *ncqo)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    /* We need all the info here */
    memcpy(self, ncqo, sizeof(su_ncqo_t));
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    /* Copy only the relevant fields */
    self->phi = ncqo->phi;
    self->omega = ncqo->omega;
    self->fnor = ncqo->fnor;

    self->sin_updated = ncqo->sin_updated;
    self->sin = ncqo->sin;

    self->cos_updated = ncqo->cos_updated;
    self->cos = ncqo->cos;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
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

SU_METHOD(su_ncqo, void, set_phase, SUFLOAT phi)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    SU_ERROR("Cannot set phase on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->phi = phi - 2 * PI * SU_FLOOR(phi / (2 * PI));
}

SU_METHOD(su_ncqo, SUFLOAT, get_i)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    return self->cos_buffer[self->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_assert_cos(self);
    return self->cos;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SU_METHOD(su_ncqo, SUFLOAT, get_q)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    return self->sin_buffer[self->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_assert_sin(self);
    return self->sin;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SU_METHOD(su_ncqo, SUCOMPLEX, get)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    return self->cos_buffer[self->p] + I * self->sin_buffer[self->p];
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    __su_ncqo_assert_cos(self);
    __su_ncqo_assert_sin(self);

    return self->cos + self->sin * I;
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
}

SU_METHOD(su_ncqo, SUFLOAT, read_i)
{
  SUFLOAT old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    old = self->cos_buffer[self->p];
    __su_ncqo_step_precalc(self);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = self->cos;

    __su_ncqo_step(self);

    self->cos_updated = SU_TRUE;
    self->sin_updated = SU_FALSE;
    self->cos = SU_COS(self->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

SU_METHOD(su_ncqo, SUFLOAT, read_q)
{
  SUFLOAT old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    old = self->sin_buffer[self->p];
    __su_ncqo_step_precalc(self);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = self->sin;

    __su_ncqo_step(self);

    self->cos_updated = SU_FALSE;
    self->sin_updated = SU_TRUE;
    self->sin = SU_SIN(self->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

SU_METHOD(su_ncqo, SUCOMPLEX, read)
{
  SUCOMPLEX old;

#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    old = self->cos_buffer[self->p] + I * self->sin_buffer[self->p];
    __su_ncqo_step_precalc(self);
  } else {
#endif /* SU_NCQO_USE_PRECALC_BUFFER */
    old = self->cos + I * self->sin;

    __su_ncqo_step(self);

    self->cos_updated = SU_TRUE;
    self->sin_updated = SU_TRUE;

    self->cos = SU_COS(self->phi);
    self->sin = SU_SIN(self->phi);
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  return old;
}

SU_METHOD(su_ncqo, void, set_angfreq, SUFLOAT omrel)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    SU_ERROR("Cannot change frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->omega = omrel;
  self->fnor = SU_ANG2NORM_FREQ(omrel);
}

SU_METHOD(su_ncqo, void, inc_angfreq, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    SU_ERROR("Cannot increase frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->omega += delta;
  self->fnor = SU_ANG2NORM_FREQ(self->omega);
}

SU_GETTER(su_ncqo, SUFLOAT, get_angfreq)
{
  return self->omega;
}

SU_METHOD(su_ncqo, void, set_freq, SUFLOAT fnor)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    SU_ERROR("Cannot change frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->fnor = fnor;
  self->omega = SU_NORM2ANG_FREQ(fnor);
}

SU_METHOD(su_ncqo, void, inc_freq, SUFLOAT delta)
{
#ifdef SU_NCQO_USE_PRECALC_BUFFER
  if (self->pre_c) {
    SU_ERROR("Cannot increase frequency on a fixed NCQO\n");
    return;
  }
#endif /* SU_NCQO_USE_PRECALC_BUFFER */

  self->fnor += delta;
  self->omega = SU_NORM2ANG_FREQ(self->fnor);
}

SU_GETTER(su_ncqo, SUFLOAT, get_freq)
{
  return self->fnor;
}
