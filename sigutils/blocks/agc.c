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
#include "agc.h"

SUPRIVATE SUBOOL
su_block_agc_ctor(void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  su_agc_t *agc = NULL;
  const struct su_agc_params *agc_params;

  if ((agc = calloc(1, sizeof (su_agc_t))) == NULL) {
    SU_ERROR("Cannot allocate AGC state");
    goto done;
  }

  agc_params = va_arg(ap, const struct su_agc_params *);

  if (!su_agc_init(agc, agc_params)) {
    SU_ERROR("Failed to initialize AGC");
    goto done;
  }

  *private = agc;

  ok = SU_TRUE;

done:
  if (!ok) {
    if (agc != NULL) {
      su_agc_finalize(agc);
      free(agc);
    }
  }

  return ok;
}

SUPRIVATE void
su_block_agc_dtor(void *private)
{
  su_agc_t *agc;

  agc = (su_agc_t *) private;

  if (agc != NULL) {
    su_agc_finalize(agc);
    free(agc);
  }
}

SUPRIVATE ssize_t
su_block_agc_acquire(void *priv, su_stream_t *out, su_block_port_t *in)
{
  su_agc_t *agc;
  ssize_t size;
  ssize_t got;

  SUCOMPLEX *start;

  agc = (su_agc_t *) priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, increment position */
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

struct sigutils_block_class su_block_class_AGC = {
    "agc", /* name */
    1,     /* in_size */
    1,     /* out_size */
    su_block_agc_ctor,    /* constructor */
    su_block_agc_dtor,    /* destructor */
    su_block_agc_acquire  /* acquire */
};
