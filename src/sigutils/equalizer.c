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

#include <sigutils/equalizer.h>

#include <stdlib.h>
#include <string.h>

#include <sigutils/log.h>

SUPRIVATE void
su_equalizer_push_x(su_equalizer_t *eq, SUCOMPLEX x)
{
  eq->x[eq->ptr++] = x;
  if (eq->ptr >= eq->params.length)
    eq->ptr = 0;
}

SUPRIVATE SUCOMPLEX
su_equalizer_eval(const su_equalizer_t *eq)
{
  SUCOMPLEX y = 0;
  unsigned int i;
  int p;

  /* Input feedback */
  p = eq->ptr - 1;
  for (i = 0; i < eq->params.length; ++i) {
    if (p < 0)
      p += eq->params.length;

    y += eq->w[i] * eq->x[p--];
  }

  return y;
}

SUPRIVATE void
su_equalizer_update_weights(su_equalizer_t *eq, SUCOMPLEX y)
{
  unsigned int i;
  int p;
  SUCOMPLEX y2;
  SUCOMPLEX err;

  y2 = y * SU_C_CONJ(y);
  err = y * (y2 - 1.);

  p = eq->ptr - 1;
  for (i = 0; i < eq->params.length; ++i) {
    if (p < 0)
      p += eq->params.length;

    eq->w[i] -= eq->params.mu * SU_C_CONJ(eq->x[p--]) * err;
  }
}

SUBOOL
su_equalizer_init(
    su_equalizer_t *eq,
    const struct sigutils_equalizer_params *params)
{
  memset(eq, 0, sizeof(su_equalizer_t));

  eq->params = *params;

  SU_TRYCATCH(eq->w = calloc(sizeof(SUCOMPLEX), params->length), goto fail);
  SU_TRYCATCH(eq->x = calloc(sizeof(SUCOMPLEX), params->length), goto fail);

  eq->w[0] = 1.; /* Identity */

  return SU_TRUE;

fail:
  su_equalizer_finalize(eq);

  return SU_FALSE;
}

void
su_equalizer_reset(su_equalizer_t *eq)
{
  memset(eq->w, 0, sizeof(SUCOMPLEX) * eq->params.length);

  eq->w[0] = 1.;
}

SUCOMPLEX
su_equalizer_feed(su_equalizer_t *eq, SUCOMPLEX x)
{
  SUCOMPLEX y;

  su_equalizer_push_x(eq, x);
  y = su_equalizer_eval(eq);

  su_equalizer_update_weights(eq, y);

  return y;
}

void
su_equalizer_finalize(su_equalizer_t *eq)
{
  if (eq->x != NULL)
    free(eq->x);

  if (eq->w != NULL)
    free(eq->w);
}
