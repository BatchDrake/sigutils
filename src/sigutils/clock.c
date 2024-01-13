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

#include <string.h>

#define SU_LOG_LEVEL "clock"

#include <sigutils/clock.h>
#include <sigutils/log.h>


/*
 * Fixed sampler
 */

SU_CONSTRUCTOR(su_sampler, SUFLOAT bnor)
{
  SU_TRYCATCH(bnor >= 0, return SU_FALSE);

  self->bnor = bnor;
  if (bnor > 0)
    self->period = 1. / bnor;
  else
    self->period = 0;

  self->phase = 0;
  self->prev = 0;
  self->phase0_rel = 0;

  return SU_TRUE;
}

SU_METHOD(su_sampler, SUBOOL, set_rate, SUFLOAT bnor)
{
  SU_TRYCATCH(bnor >= 0, return SU_FALSE);

  self->bnor = bnor;
  if (bnor > 0) {
    self->period = 1. / bnor;
    if (self->phase > self->period)
      self->phase -= self->period * SU_FLOOR(self->phase / self->period);

    self->phase0 = self->phase0_rel * self->period;
  } else {
    self->period = 0;
  }

  return SU_TRUE;
}

/* Phase is always set in a relative fashion */
SU_METHOD(su_sampler, void, set_phase, SUFLOAT phase)
{
  if (phase > 1)
    phase -= SU_FLOOR(phase);

  self->phase = self->period * phase;
}

SU_DESTRUCTOR(su_sampler)
{ /* No-op */
}

/*
 * Clock detector
 */

SU_DESTRUCTOR(su_clock_detector)
{
  su_stream_finalize(&self->sym_stream);
}

SU_CONSTRUCTOR(
    su_clock_detector,
    SUFLOAT loop_gain,
    SUFLOAT bhint,
    SUSCOUNT bufsiz)
{
  memset(self, 0, sizeof(su_clock_detector_t));

  SU_CONSTRUCT_FAIL(su_stream, &self->sym_stream, bufsiz);

  self->alpha = SU_PREFERED_CLOCK_ALPHA;
  self->beta = SU_PREFERED_CLOCK_BETA;
  self->algo = SU_CLOCK_DETECTOR_ALGORITHM_GARDNER;
  self->phi = .25;
  self->bnor = bhint;
  self->bmin = 0;
  self->bmax = 1;
  self->gain = loop_gain; /* Somehow this parameter is critical */

  return SU_TRUE;

fail:
  SU_DESTRUCT(su_clock_detector, self);

  return SU_FALSE;
}

SU_METHOD(su_clock_detector, void, set_baud, SUFLOAT bnor)
{
  self->bnor = bnor;
  self->phi = 0;
  memset(self->x, 0, sizeof(self->x));
}

SU_METHOD(su_clock_detector, SUBOOL, set_bnor_limits, SUFLOAT lo, SUFLOAT hi)
{
  if (lo > hi) {
    SU_ERROR("Invalid baud rate limits\n");
    return SU_FALSE;
  }

  if (self->bnor < self->bmin) {
    self->bnor = self->bmin;
  } else if (self->bnor > self->bmax) {
    self->bnor = self->bmax;
  }

  return SU_TRUE;
}

SU_METHOD(su_clock_detector, void, feed, SUCOMPLEX val)
{
  SUFLOAT alpha;
  SUFLOAT e;
  SUCOMPLEX p;

  if (self->algo == SU_CLOCK_DETECTOR_ALGORITHM_NONE) {
    SU_ERROR("Invalid clock detector\n");
    return;
  }

  /* Increment phase */
  self->phi += self->bnor;

  switch (self->algo) {
    case SU_CLOCK_DETECTOR_ALGORITHM_GARDNER:
      if (self->phi >= .5) {
        /* Toggle halfcycle flag */
        self->halfcycle = !self->halfcycle;

        /* Interpolate between this and previous sample */
        alpha = self->bnor * (self->phi - .5);

        p = (1 - alpha) * val + alpha * self->prev;

        self->phi -= .5;
        if (!self->halfcycle) {
          self->x[2] = self->x[0];
          self->x[0] = p;

          /* Compute error signal */
          e = self->gain
              * SU_C_REAL(SU_C_CONJ(self->x[1]) * (self->x[0] - self->x[2]));
          self->e = e;
          /* Adjust phase and frequency */
          self->phi += self->alpha * e;
          self->bnor += self->beta * e;

          /* Check that current baudrate is within some reasonable limits */
          if (self->bnor > self->bmax)
            self->bnor = self->bmax;
          if (self->bnor < self->bmin)
            self->bnor = self->bmin;

          su_stream_write(&self->sym_stream, &p, 1);
        } else {
          self->x[1] = p;
        }
      }
      break;

    default:
      SU_ERROR("Unsupported clock detection algorithm\n");
  }

  self->prev = val;
}

SU_METHOD(su_clock_detector, SUSDIFF, read, SUCOMPLEX *buf, size_t size)
{
  SUSDIFF result = 0;

  result = su_stream_read(&self->sym_stream, self->sym_stream_pos, buf, size);

  if (result < 0) {
    SU_WARNING("Symbols lost, resync requested\n");
    self->sym_stream_pos = su_stream_tell(&self->sym_stream);
    result = 0;
  }

  self->sym_stream_pos += result;

  return result;
}
