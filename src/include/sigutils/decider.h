/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

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

#ifndef _DECIDER_H
#define _DECIDER_H

#include <sigutils/defs.h>
#include <sigutils/types.h>

struct sigutils_decider_params {
  SUFLOAT min_val;
  SUFLOAT max_val;
  unsigned int bits;
};

#define sigutils_decider_params_INITIALIZER \
  {                                         \
    -PI,    /* min_val */                   \
        PI, /* max_val */                   \
        1,  /* bits */                      \
  }

struct sigutils_decider {
  struct sigutils_decider_params params;
  SUFLOAT width;
  SUFLOAT h_inv; /* 2 ^ bits / width */
  SUBITS mask;
};

typedef struct sigutils_decider su_decider_t;

SU_GETTER(su_decider, const struct sigutils_decider_params *, get_params);

const struct sigutils_decider_params *
su_decider_get_params(const su_decider_t *self)
{
  return &self->params;
}

SUBOOL
su_decider_init(
    su_decider_t *decider,
    const struct sigutils_decider_params *params)
{
  if (params->bits > 8)
    return SU_FALSE;

  if (params->min_val >= params->max_val)
    return SU_FALSE;

  decider->params = *params;
  decider->width = params->max_val - params->min_val;
  decider->mask = (1 << params->bits) - 1;
  decider->h_inv = (1 << params->bits) / decider->width;

  return SU_TRUE;
}

SUINLINE SUBITS
su_decider_decide(const su_decider_t *decider, SUFLOAT x)
{
  x -= decider->params.min_val;

  if (x < 0)
    return 0;
  else if (x >= decider->width)
    return decider->mask;
  else
    return (SUBITS)SU_FLOOR(x * decider->h_inv);
}

SUINLINE SUBITS
su_decider_decide_cyclic(const su_decider_t *decider, SUFLOAT x)
{
  x -= decider->params.min_val;

  return decider->mask & (SUBITS)SU_FLOOR(x * decider->h_inv);
}

#endif /* _DECIDER_H */
