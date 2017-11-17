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

#include <string.h>
#include <stdlib.h>

#define SU_LOG_LEVEL "diff-encoder"

#include "log.h"
#include "encoder.h"

struct su_diff_encoder_state {
  SUSYMBOL prev;
  SUBOOL   sign;
  SUBITS   mask;
};

SUPRIVATE SUBOOL
su_diff_encoder_ctor(su_encoder_t *encoder, void **private, va_list ap)
{
  struct su_diff_encoder_state *new;

  SU_TRYCATCH(
      new = malloc(sizeof (struct su_diff_encoder_state)),
      return SU_FALSE);

  new->sign = va_arg(ap, SUBOOL);
  new->prev = SU_NOSYMBOL;
  new->mask = (1 << encoder->bits) - 1;

  *private = new;

  return SU_TRUE;
}

SUINLINE SUBITS
su_diff_encoder_int(const struct su_diff_encoder_state *s, SUBITS a, SUBITS b)
{
  if (s->sign)
    return s->mask & (a - b);
  else
    return s->mask & (a + b);
}

SUINLINE SUBITS
su_diff_encoder_diff(const struct su_diff_encoder_state *s, SUBITS a, SUBITS b)
{
  if (s->sign)
    return s->mask & (b - a);
  else
    return s->mask & (a - b);
}

SUPRIVATE SUSYMBOL
su_diff_encoder_encode(su_encoder_t *encoder, void *private, SUSYMBOL x)
{
  struct su_diff_encoder_state *state =
      (struct su_diff_encoder_state *) private;
  SUSYMBOL y;

  if (SU_ISSYM(x)) {
    if (state->prev != SU_NOSYMBOL) {
      y = SU_TOSYM(
            su_diff_encoder_int(
                state, /* Encode == Ambiguously integrate */
                SU_FROMSYM(state->prev),
                SU_FROMSYM(x)));

    } else {
      y = x;
    }
    state->prev = y;
  } else {
    /* When we don't receive a symbol, we must reset the stream */
    y = x;
    state->prev = SU_NOSYMBOL;
  }

  return y;
}

SUPRIVATE SUSYMBOL
su_diff_encoder_decode(su_encoder_t *encoder, void *private, SUSYMBOL x)
{
  struct su_diff_encoder_state *state =
      (struct su_diff_encoder_state *) private;
  SUSYMBOL y;

  if (SU_ISSYM(x)) {
    if (state->prev != SU_NOSYMBOL) {
      y = SU_TOSYM(
            su_diff_encoder_diff(
                state, /* Decode == Unambiguously differentiate */
                SU_FROMSYM(state->prev),
                SU_FROMSYM(x)));
    } else {
      y = SU_NOSYMBOL;
    }

    state->prev = x;
  } else {
    /* When we don't receive a symbol, we must reset the stream */
    y = x;
    state->prev = SU_NOSYMBOL;
  }

  return y;
}

SUPRIVATE void
su_diff_encoder_dtor(void *private)
{
  free(private);
}

struct sigutils_encoder_class su_encoder_class_DIFF = {
    .name   = "diff",
    .ctor   = su_diff_encoder_ctor,
    .dtor   = su_diff_encoder_dtor,
    .encode = su_diff_encoder_encode,
    .decode = su_diff_encoder_decode,
};
