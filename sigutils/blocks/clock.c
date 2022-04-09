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

#define SU_LOG_DOMAIN "clock-block"

#include "block.h"
#include "clock.h"
#include "log.h"

SUPRIVATE SUBOOL
su_block_cdr_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  su_clock_detector_t *clock_detector = NULL;
  /* Constructor params */
  SUFLOAT loop_gain = 0;
  SUFLOAT bhint = 0;
  SUSCOUNT bufsiz = 0;

  if ((clock_detector = calloc(1, sizeof(su_clock_detector_t))) == NULL) {
    SU_ERROR("Cannot allocate clock detector state");
    goto done;
  }

  /* Variadic function calls promote floats to doubles */
  loop_gain = va_arg(ap, double);
  bhint = va_arg(ap, double);
  bufsiz = va_arg(ap, SUSCOUNT);

  if (!su_clock_detector_init(clock_detector, loop_gain, bhint, bufsiz)) {
    SU_ERROR("Failed to initialize Costas loop");
    goto done;
  }

  ok = SU_TRUE;

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "bnor",
                                    &clock_detector->bnor);

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "bmax",
                                    &clock_detector->bmax);

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "bmin",
                                    &clock_detector->bmin);

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "alpha",
                                    &clock_detector->alpha);

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "beta",
                                    &clock_detector->beta);

  ok = ok
       && su_block_set_property_ref(block,
                                    SU_PROPERTY_TYPE_FLOAT,
                                    "gain",
                                    &clock_detector->gain);

done:
  if (!ok) {
    if (clock_detector != NULL) {
      su_clock_detector_finalize(clock_detector);
      free(clock_detector);
    }
  } else
    *private = clock_detector;

  return ok;
}

SUPRIVATE void
su_block_cdr_dtor(void *private)
{
  su_clock_detector_t *clock_detector;

  clock_detector = (su_clock_detector_t *)private;

  if (clock_detector != NULL) {
    su_clock_detector_finalize(clock_detector);
    free(clock_detector);
  }
}

SUPRIVATE SUSDIFF
su_block_cdr_acquire(void *priv,
                     su_stream_t *out,
                     unsigned int port_id,
                     su_block_port_t *in)
{
  su_clock_detector_t *clock_detector;
  SUSDIFF size;
  SUSDIFF got;
  int i = 0;
  int p = 0;
  SUCOMPLEX *start;

  clock_detector = (su_clock_detector_t *)priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, process in place */
      p = 0;
      for (i = 0; i < got; ++i) {
        su_clock_detector_feed(clock_detector, start[i]);
        p += su_clock_detector_read(clock_detector, start + p, 1);
      }

      /* Increment position */
      if (su_stream_advance_contiguous(out, p) != p) {
        SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
        return -1;
      }
    } else if (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC) {
      SU_WARNING("Clock detector slow, samples lost\n");
      if (!su_block_port_resync(in)) {
        SU_ERROR("Failed to resync\n");
        return -1;
      }
    } else if (got < 0) {
      SU_ERROR("su_block_port_read: error %d\n", got);
      return -1;
    }
  } while (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC || (p == 0 && got > 0));

  return p;
}

struct sigutils_block_class su_block_class_CDR = {
    "cdr",               /* name */
    1,                   /* in_size */
    1,                   /* out_size */
    su_block_cdr_ctor,   /* constructor */
    su_block_cdr_dtor,   /* destructor */
    su_block_cdr_acquire /* acquire */
};
