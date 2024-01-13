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

#include <sigutils/agc.h>

#include <stdlib.h>
#include <string.h>

#include <sigutils/log.h>

SU_CONSTRUCTOR(su_agc, const struct su_agc_params *params)
{
  memset(self, 0, sizeof(su_agc_t));

  SU_ALLOCATE_MANY_FAIL(self->mag_history, params->mag_history_size, SUFLOAT);

  SU_ALLOCATE_MANY_FAIL(self->delay_line, params->delay_line_size, SUCOMPLEX);

  self->mag_history_size = params->mag_history_size;
  self->delay_line_size = params->delay_line_size;
  self->knee = params->threshold;
  self->hang_max = params->hang_max;
  self->gain_slope = params->slope_factor * 1e-2;
  self->fast_alpha_rise = 1 - SU_EXP(-1. / params->fast_rise_t);
  self->fast_alpha_fall = 1 - SU_EXP(-1. / params->fast_fall_t);
  self->slow_alpha_rise = 1 - SU_EXP(-1. / params->slow_rise_t);
  self->slow_alpha_fall = 1 - SU_EXP(-1. / params->slow_fall_t);
  self->fixed_gain = SU_MAG_RAW(self->knee * (self->gain_slope - 1));

  self->enabled = SU_TRUE;

  return SU_TRUE;

fail:
  SU_DESTRUCT(su_agc, self);

  return SU_FALSE;
}

SU_DESTRUCTOR(su_agc)
{
  if (self->mag_history != NULL)
    free(self->mag_history);

  if (self->delay_line != NULL)
    free(self->delay_line);
}

/*
 *  AGC Algorithm (inspired by GQRX's AGC)
 *
 *  1. Put x in delay buffer and magnitude in dBs
 *  2. Retrieve oldest sample and magnitude in dBs.
 *  3. Compute peak value.
 *  4. Update averagers
 *  5. In decay averager: update only if hangtime has elapsed
 *  6. Get magnitude from the biggest of both averagers
 *  7. Output sample
 */

SU_METHOD(su_agc, SUCOMPLEX, feed, SUCOMPLEX x)
{
  unsigned int i;

  SUCOMPLEX x_delayed;
  SUFLOAT x_dBFS;
  SUFLOAT x_dBFS_delayed;
  SUFLOAT peak_delta;

  /* Push sample */
  x_delayed = self->delay_line[self->delay_line_ptr];

  self->delay_line[self->delay_line_ptr++] = x;
  if (self->delay_line_ptr >= self->delay_line_size)
    self->delay_line_ptr = 0;

  if (self->enabled) {
    x_dBFS = .5 * SU_DB(x * SU_C_CONJ(x)) - SUFLOAT_MAX_REF_DB;

    /* Push mag */
    x_dBFS_delayed = self->mag_history[self->mag_history_ptr];

    self->mag_history[self->mag_history_ptr++] = x_dBFS;
    if (self->mag_history_ptr >= self->mag_history_size)
      self->mag_history_ptr = 0;

    if (x_dBFS > self->peak)
      self->peak = x_dBFS;
    else if (self->peak == x_dBFS_delayed) {
      /*
       * We've just removed the peak value from the magnitude history, we
       * need to recalculate the current peak value.
       */
      self->peak = SUFLOAT_MIN_REF_DB;

      for (i = 0; i < self->mag_history_size; ++i) {
        if (self->peak < self->mag_history[i])
          self->peak = self->mag_history[i];
      }
    }

    /* Update levels for fast averager */
    peak_delta = self->peak - self->fast_level;
    if (peak_delta > 0)
      self->fast_level += self->fast_alpha_rise * peak_delta;
    else
      self->fast_level += self->fast_alpha_fall * peak_delta;

    /* Update levels for slow averager */
    peak_delta = self->peak - self->slow_level;
    if (peak_delta > 0) {
      self->slow_level += self->slow_alpha_rise * peak_delta;
      self->hang_n = 0;
    } else if (self->hang_n >= self->hang_max)
      self->slow_level += self->slow_alpha_fall * peak_delta;
    else
      ++self->hang_n;

    /* Keep biggest magnitude */
    x_dBFS_delayed = SU_MAX(self->fast_level, self->slow_level);

    /* Is AGC on? */
    if (x_dBFS_delayed < self->knee)
      x_delayed *= self->fixed_gain;
    else
      x_delayed *= SU_MAG_RAW(x_dBFS_delayed * (self->gain_slope - 1));

    x_delayed *= SU_AGC_RESCALE;
  }

  return x_delayed;
}
