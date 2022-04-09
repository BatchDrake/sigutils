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

#include <stdlib.h>
#include <string.h>

#define SU_LOG_LEVEL "siggen-block"

#include "block.h"
#include "log.h"

enum su_sig_type {
  SU_SIGNAL_TYPE_NULL,
  SU_SIGNAL_TYPE_SINE,
  SU_SIGNAL_TYPE_COSINE,
  SU_SIGNAL_TYPE_SQUARE,
  SU_SIGNAL_TYPE_SAWTOOTH,
  SU_SIGNAL_TYPE_NOISE
};

struct su_sig_desc {
  enum su_sig_type type;
  SUFLOAT A;  /* Amplitude */
  SUSCOUNT T; /* Period */
  SUSCOUNT n; /* Phase */
};

struct su_siggen_state {
  struct su_sig_desc i_desc;
  struct su_sig_desc q_desc;
};

SUPRIVATE SUFLOAT
su_sig_desc_eval(const struct su_sig_desc *desc)
{
  SUFLOAT y = nan("error");

  switch (desc->type) {
    case SU_SIGNAL_TYPE_NULL:
      y = 0.0;
      break;

    case SU_SIGNAL_TYPE_SINE:
      y = sin(2. * M_PI * (SUFLOAT)(desc->n % desc->T) / (SUFLOAT)desc->T);
      break;

    case SU_SIGNAL_TYPE_COSINE:
      y = cos(2. * M_PI * (SUFLOAT)(desc->n % desc->T) / (SUFLOAT)desc->T);
      break;

    case SU_SIGNAL_TYPE_SQUARE:
      y = desc->n >= (desc->T >> 1);
      break;

    case SU_SIGNAL_TYPE_SAWTOOTH:
      y = (SUFLOAT)(desc->n % desc->T) / (SUFLOAT)desc->T;
      break;

    case SU_SIGNAL_TYPE_NOISE:
      y = SU_C_REAL(su_c_awgn());
      break;
  }

  return desc->A * y;
}

SUPRIVATE void
su_sig_desc_advance(struct su_sig_desc *desc)
{
  ++desc->n;
}

SUPRIVATE SUCOMPLEX
su_siggen_read(struct su_siggen_state *state)
{
  SUCOMPLEX y;

  y = su_sig_desc_eval(&state->i_desc) + I * su_sig_desc_eval(&state->q_desc);

  su_sig_desc_advance(&state->i_desc);
  su_sig_desc_advance(&state->q_desc);

  return y;
}

SUPRIVATE SUBOOL
su_block_siggen_string_to_sig_type(const char *str, enum su_sig_type *type)
{
  if (strcmp(str, "null") == 0)
    *type = SU_SIGNAL_TYPE_NULL;
  else if (strcmp(str, "sin") == 0)
    *type = SU_SIGNAL_TYPE_SINE;
  else if (strcmp(str, "cos") == 0)
    *type = SU_SIGNAL_TYPE_COSINE;
  else if (strcmp(str, "square") == 0)
    *type = SU_SIGNAL_TYPE_SQUARE;
  else if (strcmp(str, "sawtooth") == 0)
    *type = SU_SIGNAL_TYPE_SAWTOOTH;
  else if (strcmp(str, "noise") == 0)
    *type = SU_SIGNAL_TYPE_NOISE;
  else
    return SU_FALSE;

  return SU_TRUE;
}

SUPRIVATE SUBOOL
su_block_siggen_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  struct su_siggen_state *state = NULL;
  const char *typestr;
  SUBOOL result = SU_FALSE;

  if ((state = calloc(1, sizeof(struct su_siggen_state))) == NULL)
    goto done;

  typestr = va_arg(ap, const char *);
  if (!su_block_siggen_string_to_sig_type(typestr, &state->i_desc.type)) {
    SU_ERROR("invalid signal type `%s' for I channel\n", typestr);
    goto done;
  }

  state->i_desc.A = va_arg(ap, double);
  state->i_desc.T = va_arg(ap, SUSCOUNT);
  state->i_desc.n = va_arg(ap, SUSCOUNT);

  typestr = va_arg(ap, const char *);
  if (!su_block_siggen_string_to_sig_type(typestr, &state->q_desc.type)) {
    SU_ERROR("invalid signal type `%s' for Q channel\n", typestr);
    goto done;
  }

  state->q_desc.A = va_arg(ap, double);
  state->q_desc.T = va_arg(ap, SUSCOUNT);
  state->q_desc.n = va_arg(ap, SUSCOUNT);

  result = SU_TRUE;

done:
  if (result)
    *private = state;
  else if (state != NULL)
    free(state);

  return result;
}

SUPRIVATE void
su_block_siggen_dtor(void *private)
{
  free(private);
}

SUPRIVATE SUSDIFF
su_block_siggen_acquire(void *priv,
                        su_stream_t *out,
                        unsigned int port_id,
                        su_block_port_t *in)
{
  struct su_siggen_state *state = (struct su_siggen_state *)priv;
  SUSDIFF size;
  unsigned int i;
  SUCOMPLEX *start;

  /* Get the number of complex samples to write */
  size = su_stream_get_contiguous(out, &start, out->size);

  for (i = 0; i < size; ++i)
    start[i] = su_siggen_read(state);

  su_stream_advance_contiguous(out, size);

  return size;
}

struct sigutils_block_class su_block_class_SIGGEN = {
    "siggen",                /* name */
    0,                       /* in_size */
    1,                       /* out_size */
    su_block_siggen_ctor,    /* constructor */
    su_block_siggen_dtor,    /* destructor */
    su_block_siggen_acquire, /* acquire */
};
