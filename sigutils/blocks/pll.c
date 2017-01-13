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
#include <stdlib.h>

#include "block.h"
#include "pll.h"

SUPRIVATE SUBOOL
su_block_costas_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  su_costas_t *costas = NULL;
  /* Constructor params */
  enum sigutils_costas_kind kind;
  SUFLOAT fhint = 0;
  SUFLOAT arm_bw = 0;
  unsigned int arm_order = 0;
  SUFLOAT loop_bw = 0;

  if ((costas = calloc(1, sizeof (su_costas_t))) == NULL) {
    SU_ERROR("Cannot allocate Costas loop state");
    goto done;
  }

  kind      = va_arg(ap, enum sigutils_costas_kind);
  fhint     = va_arg(ap, SUFLOAT);
  arm_bw    = va_arg(ap, SUFLOAT);
  arm_order = va_arg(ap, unsigned int);
  loop_bw   = va_arg(ap, SUFLOAT);

  if (!su_costas_init(costas, kind, fhint, arm_bw, arm_order, loop_bw)) {
    SU_ERROR("Failed to initialize Costas loop");
    goto done;
  }

  ok = SU_TRUE;

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "f",
      &costas->ncqo.fnor);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "lock",
      &costas->lock);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "beta",
      &costas->b);

done:
  if (!ok) {
    if (costas != NULL) {
      su_costas_finalize(costas);
      free(costas);
    }
  }
  else
    *private = costas;

  return ok;
}

SUPRIVATE void
su_block_costas_dtor(void *private)
{
  su_costas_t *costas;

  costas = (su_costas_t *) private;

  if (costas != NULL) {
    su_costas_finalize(costas);
    free(costas);
  }
}

SUPRIVATE ssize_t
su_block_costas_acquire(void *priv, su_stream_t *out, su_block_port_t *in)
{
  su_costas_t *costas;
  ssize_t size;
  ssize_t got;
  int i = 0;

  SUCOMPLEX *start;

  costas = (su_costas_t *) priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, process in place */
      for (i = 0; i < got; ++i) {
        su_costas_feed(costas, start[i]);
        start[i] = costas->y;
      }

      /* Increment position */
      if (su_stream_advance_contiguous(out, got) != got) {
        SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
        return -1;
      }
    } else if (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC) {
      SU_WARNING("AGC slow, samples lost\n");
      if (!su_block_port_resync(in)) {
        SU_ERROR("Failed to resync\n");
        return -1;
      }
    } else if (got < 0) {
      SU_ERROR("su_block_port_read: error %d\n", got);
      return -1;
    }
  } while (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC);

  return got;
}

struct sigutils_block_class su_block_class_COSTAS = {
    "costas",   /* name */
    1,          /* in_size */
    1,          /* out_size */
    su_block_costas_ctor,    /* constructor */
    su_block_costas_dtor,    /* destructor */
    su_block_costas_acquire  /* acquire */
};
