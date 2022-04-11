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
#include <stdlib.h>

#define SU_LOG_LEVEL "agc-block"

#include "agc.h"
#include "block.h"
#include "log.h"

SUPRIVATE SUBOOL
su_block_agc_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  su_agc_t *agc = NULL;
  const struct su_agc_params *agc_params;

  if ((agc = calloc(1, sizeof(su_agc_t))) == NULL) {
    SU_ERROR("Cannot allocate AGC state");
    goto done;
  }

  agc_params = va_arg(ap, const struct su_agc_params *);

  if (!su_agc_init(agc, agc_params)) {
    SU_ERROR("Failed to initialize AGC");
    goto done;
  }

  ok = SU_TRUE;

  ok = ok
       && su_block_set_property_ref(
           block,
           SU_PROPERTY_TYPE_FLOAT,
           "peak",
           &agc->peak);

  ok = ok
       && su_block_set_property_ref(
           block,
           SU_PROPERTY_TYPE_BOOL,
           "enabled",
           &agc->enabled);

done:
  if (!ok) {
    if (agc != NULL) {
      su_agc_finalize(agc);
      free(agc);
    }
  } else
    *private = agc;

  return ok;
}

SUPRIVATE void
su_block_agc_dtor(void *private)
{
  su_agc_t *agc;

  agc = (su_agc_t *)private;

  if (agc != NULL) {
    su_agc_finalize(agc);
    free(agc);
  }
}

SUPRIVATE SUSDIFF
su_block_agc_acquire(
    void *priv,
    su_stream_t *out,
    unsigned int port_id,
    su_block_port_t *in)
{
  su_agc_t *agc;
  SUSDIFF size;
  SUSDIFF got;
  int i = 0;

  SUCOMPLEX *start;

  agc = (su_agc_t *)priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, process in place */
      for (i = 0; i < got; ++i)
        start[i] = su_agc_feed(agc, start[i]);

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

struct sigutils_block_class su_block_class_AGC = {
    "agc",               /* name */
    1,                   /* in_size */
    1,                   /* out_size */
    su_block_agc_ctor,   /* constructor */
    su_block_agc_dtor,   /* destructor */
    su_block_agc_acquire /* acquire */
};
