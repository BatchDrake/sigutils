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

#define SU_LOG_DOMAIN "block"

#include "block.h"
#include "iir.h"
#include "log.h"
#include "taps.h"

SUPRIVATE SUBOOL
su_block_rrc_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  su_iir_filt_t *filt = NULL;
  unsigned int order = 0;
  SUFLOAT T = 0;
  SUFLOAT beta = 0;

  if ((filt = calloc(1, sizeof(su_iir_filt_t))) == NULL) {
    SU_ERROR("Cannot allocate RRC filter state\n");
    goto done;
  }

  order = va_arg(ap, unsigned int);
  T = va_arg(ap, double);
  beta = va_arg(ap, double);

  if (!su_iir_rrc_init(filt, order, T, beta)) {
    SU_ERROR("Failed to initialize RRC filter\n");
    goto done;
  }

  ok = su_block_set_property_ref(block,
                                 SU_PROPERTY_TYPE_FLOAT,
                                 "gain",
                                 &filt->gain);

done:
  if (!ok) {
    if (filt != NULL) {
      su_iir_filt_finalize(filt);
      free(filt);
    }
  } else
    *private = filt;

  return ok;
}

SUPRIVATE void
su_block_rrc_dtor(void *private)
{
  su_iir_filt_t *filt;

  filt = (su_iir_filt_t *)private;

  if (filt != NULL) {
    su_iir_filt_finalize(filt);
    free(filt);
  }
}

SUPRIVATE SUSDIFF
su_block_rrc_acquire(void *priv,
                     su_stream_t *out,
                     unsigned int port_id,
                     su_block_port_t *in)
{
  su_iir_filt_t *filt;
  SUSDIFF size;
  SUSDIFF got;
  int i = 0;

  SUCOMPLEX *start;

  filt = (su_iir_filt_t *)priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, process in place */
      for (i = 0; i < got; ++i)
        start[i] = su_iir_filt_feed(filt, start[i]);

      /* Increment position */
      if (su_stream_advance_contiguous(out, got) != got) {
        SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
        return -1;
      }
    } else if (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC) {
      SU_WARNING("RRC filter slow, samples lost\n");
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

struct sigutils_block_class su_block_class_RRC = {
    "rrc",               /* name */
    1,                   /* in_size */
    1,                   /* out_size */
    su_block_rrc_ctor,   /* constructor */
    su_block_rrc_dtor,   /* destructor */
    su_block_rrc_acquire /* acquire */
};
