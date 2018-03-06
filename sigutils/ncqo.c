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

#include <math.h>

#include "ncqo.h"
#include "sampling.h"

/* Expects: relative frequency */
void
su_ncqo_init(su_ncqo_t *ncqo, SUFLOAT fnor)
{
  ncqo->n     = 0;
  ncqo->phi   = .0;
  ncqo->omega = SU_NORM2ANG_FREQ(fnor);
  ncqo->fnor  = fnor;
  ncqo->sin   = 0;
  ncqo->cos   = 1;
}

SUPRIVATE void
__su_ncqo_assert_cos(su_ncqo_t *ncqo)
{
  if (!ncqo->cos_updated) {
    ncqo->cos = SU_COS(ncqo->phi);
    ++ncqo->cos_updated;
  }
}

SUPRIVATE void
__su_ncqo_assert_sin(su_ncqo_t *ncqo)
{
  if (!ncqo->sin_updated) {
    ncqo->sin = SU_SIN(ncqo->phi);
    ++ncqo->sin_updated;
  }
}

SUPRIVATE void
__su_ncqo_step(su_ncqo_t *ncqo)
{
  ++ncqo->n;
  ncqo->phi += ncqo->omega;

  if (ncqo->phi >= 2 * PI)
    ncqo->phi -= 2 * PI;
  else if (ncqo->phi < 0)
    ncqo->phi += 2 * PI;
}

void
su_ncqo_step(su_ncqo_t *ncqo)
{
  __su_ncqo_step(ncqo);

  /* Sine & cosine values are now outdated */
  ncqo->cos_updated = SU_FALSE;
  ncqo->sin_updated = SU_FALSE;
}

void
su_ncqo_set_phase(su_ncqo_t *ncqo, SUFLOAT phi)
{
  ncqo->phi = phi - 2 * PI * SU_FLOOR(phi / (2 * PI));
}

SUFLOAT
su_ncqo_get_phase(su_ncqo_t *ncqo)
{
  return ncqo->phi;
}

void
su_ncqo_inc_phase(su_ncqo_t *ncqo, SUFLOAT delta)
{
  ncqo->phi += delta;

  if (ncqo->phi < 0 || ncqo->phi >= 2 * PI) {
    ncqo->phi -= 2 * PI * SU_FLOOR(ncqo->phi / (2 * PI));
  }
}

SUFLOAT
su_ncqo_get_i(su_ncqo_t *ncqo)
{
  __su_ncqo_assert_cos(ncqo);
  return ncqo->cos;
}

SUFLOAT
su_ncqo_get_q(su_ncqo_t *ncqo)
{
  __su_ncqo_assert_sin(ncqo);
  return ncqo->sin;
}

SUCOMPLEX
su_ncqo_get(su_ncqo_t *ncqo)
{
  __su_ncqo_assert_cos(ncqo);
  __su_ncqo_assert_sin(ncqo);

  return ncqo->cos + ncqo->sin * I;
}

SUFLOAT
su_ncqo_read_i(su_ncqo_t *ncqo)
{
  __su_ncqo_step(ncqo);

  ncqo->cos_updated = SU_TRUE;
  ncqo->sin_updated = SU_FALSE;
  ncqo->cos = SU_COS(ncqo->phi);

  return ncqo->cos;
}

SUFLOAT
su_ncqo_read_q(su_ncqo_t *ncqo)
{
  __su_ncqo_step(ncqo);

  ncqo->cos_updated = SU_FALSE;
  ncqo->sin_updated = SU_TRUE;
  ncqo->sin = SU_SIN(ncqo->phi);

  return ncqo->sin;
}

SUCOMPLEX
su_ncqo_read(su_ncqo_t *ncqo)
{
  __su_ncqo_step(ncqo);

  ncqo->cos_updated = SU_TRUE;
  ncqo->sin_updated = SU_TRUE;

  ncqo->cos = SU_COS(ncqo->phi);
  ncqo->sin = SU_SIN(ncqo->phi);

  return ncqo->cos + I * ncqo->sin;
}

void
su_ncqo_set_angfreq(su_ncqo_t *ncqo, SUFLOAT omrel)
{
  ncqo->omega = omrel;
  ncqo->fnor  = SU_ANG2NORM_FREQ(omrel);
}

void
su_ncqo_inc_angfreq(su_ncqo_t *ncqo, SUFLOAT delta)
{
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
  ncqo->fnor  = fnor;
  ncqo->omega = SU_NORM2ANG_FREQ(fnor);
}

void
su_ncqo_inc_freq(su_ncqo_t *ncqo, SUFLOAT delta)
{
  ncqo->fnor  += delta;
  ncqo->omega  = SU_NORM2ANG_FREQ(ncqo->fnor);
}

SUFLOAT
su_ncqo_get_freq(const su_ncqo_t *ncqo)
{
  return ncqo->fnor;
}
