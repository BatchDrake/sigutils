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

#include "clock.h"

void
su_clock_detector_finalize(su_clock_detector_t *cd)
{
  su_stream_finalize(&cd->sym_stream);
}

SUBOOL
su_clock_detector_init(
    su_clock_detector_t *cd,
    SUFLOAT loop_gain,
    SUFLOAT bhint,
    SUSCOUNT bufsiz)
{
  memset(cd, 0, sizeof(su_clock_detector_t));

  if (!su_stream_init(&cd->sym_stream, bufsiz)) {
    SU_ERROR("Failed to initialize output stream\n");
    goto fail;
  }

  cd->alpha = SU_PREFERED_CLOCK_ALPHA;
  cd->beta  = SU_PREFERED_CLOCK_BETA;
  cd->algo  = SU_CLOCK_DETECTOR_ALGORITHM_GARDNER;
  cd->phi   = .25;
  cd->bnor  = bhint;
  cd->bmin  = 0;
  cd->bmax  = 1;
  cd->gain  = loop_gain; /* Somehow this parameter is critical */

  return SU_TRUE;

fail:
  su_clock_detector_finalize(cd);

  return SU_FALSE;
}

SUBOOL
su_clock_detector_set_bnor_limits(
    su_clock_detector_t *cd,
    SUFLOAT lo,
    SUFLOAT hi)
{
  if (lo > hi) {
    SU_ERROR("Invalid baud rate limits\n");
    return SU_FALSE;
  }

  if (cd->bnor < cd->bmin) {
    cd->bnor = cd->bmin;
  } else if (cd->bnor > cd->bmax) {
    cd->bnor = cd->bmax;
  }

  return SU_TRUE;
}

void
su_clock_detector_feed(su_clock_detector_t *cd, SUCOMPLEX val)
{
  SUFLOAT alpha;
  SUFLOAT e;
  SUCOMPLEX p;

  if (cd->algo == SU_CLOCK_DETECTOR_ALGORITHM_NONE) {
    SU_ERROR("Invalid clock detector\n");
    return;
  }

  /* Increment phase */
  cd->phi += cd->bnor;

  switch (cd->algo) {
    case SU_CLOCK_DETECTOR_ALGORITHM_GARDNER:
      if (cd->phi >= .5) {
        /* Toggle halfcycle flag */
        cd->halfcycle = !cd->halfcycle;

        /* Interpolate between this and previous sample */
        alpha = cd->bnor * (cd->phi - .5);

        p = (1 - alpha) * val + alpha * cd->prev;

        cd->phi -= .5;
        if (!cd->halfcycle) {
          cd->x[2] = cd->x[0];
          cd->x[0] = p;

          /* Compute error signal */
          e = cd->gain * SU_C_REAL(SU_C_CONJ(cd->x[1]) * (cd->x[0] - cd->x[2]));
          cd->e = e;
          /* Adjust phase and frequency */
          cd->phi  += cd->alpha * e;
          cd->bnor += cd->beta * e;

          /* Check that current baudrate is within some reasonable limits */
          if (cd->bnor > cd->bmax)
            cd->bnor = cd->bmax;
          if (cd->bnor < cd->bmin)
            cd->bnor = cd->bmin;

          su_stream_write(&cd->sym_stream, &p, 1);
        } else {
          cd->x[1] = p;
        }
      }
      break;

    default:
      SU_ERROR("Unsupported clock detection algorithm\n");
  }

  cd->prev = val;
}

ssize_t
su_clock_detector_read(su_clock_detector_t *cd, SUCOMPLEX *buf, size_t size)
{
  ssize_t result = 0;

  result = su_stream_read(&cd->sym_stream, cd->sym_stream_pos, buf, size);

  if (result < 0) {
    SU_WARNING("Symbols lost, resync requested\n");
    cd->sym_stream_pos = su_stream_tell(&cd->sym_stream);
    result = 0;
  }

  cd->sym_stream_pos += result;

  return result;
}
