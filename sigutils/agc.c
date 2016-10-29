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
#include <string.h>
#include "agc.h"

SUBOOL
su_agc_init(su_agc_t *agc, const struct su_agc_params *params)
{
  SUFLOAT *mag_buf = NULL;
  SUFLOAT *delay_line = NULL;

  memset(agc, 0, sizeof (su_agc_t));

  if ((mag_buf = calloc(params->mag_history_size, sizeof (SUFLOAT))) == NULL)
    goto fail;

  if ((delay_line = calloc(params->delay_line_size, sizeof (SUFLOAT))) == NULL)
    goto fail;

  agc->mag_history      = mag_buf;
  agc->delay_line       = delay_line;
  agc->mag_history_size = params->mag_history_size;
  agc->delay_line_size  = params->delay_line_size;
  agc->knee             = params->threshold;
  agc->hang_max         = params->hang_max;
  agc->gain_slope       = params->slope_factor * 1e-2;
  agc->fast_alpha_rise  = 1 - SU_EXP(-1. / params->fast_rise_t);
  agc->fast_alpha_fall  = 1 - SU_EXP(-1. / params->fast_fall_t);
  agc->slow_alpha_rise  = 1 - SU_EXP(-1. / params->slow_rise_t);
  agc->slow_alpha_fall  = 1 - SU_EXP(-1. / params->slow_fall_t);
  agc->fixed_gain       = SU_MAG_RAW(agc->knee * (agc->gain_slope - 1));

  agc->enabled          = SU_TRUE;

  return SU_TRUE;

fail:

  su_agc_finalize(agc);

  return SU_FALSE;
}

void
su_agc_finalize(su_agc_t *agc)
{
  if (agc->mag_history != NULL)
    free(agc->mag_history);

  if (agc->delay_line != NULL)
    free(agc->delay_line);
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

SUFLOAT
su_agc_feed(su_agc_t *agc, SUFLOAT x)
{
  unsigned int i;

  SUFLOAT x_delayed;
  SUFLOAT x_dBFS;
  SUFLOAT x_dBFS_delayed;
  SUFLOAT peak_delta;

  /* Push sample */
  x_delayed = agc->delay_line[agc->delay_line_ptr];

  agc->delay_line[agc->delay_line_ptr++] = x;
  if (agc->delay_line_ptr >= agc->delay_line_size)
    agc->delay_line_ptr = 0;

  if (agc->enabled) {
    x_dBFS = SU_DB(SU_ABS(x)) - SUFLOAT_MAX_REF_DB;

    /* Push mag */
    x_dBFS_delayed = agc->mag_history[agc->mag_history_ptr];

    agc->mag_history[agc->mag_history_ptr++] = x_dBFS;
    if (agc->mag_history_ptr >= agc->mag_history_size)
      agc->mag_history_ptr = 0;

    if (x_dBFS > agc->peak)
      agc->peak = x_dBFS;
    else if (agc->peak == x_dBFS_delayed) {
      /*
       * We've just removed the peak value from the magnitude history, we
       * need to recalculate the current peak value.
       */
      agc->peak = SUFLOAT_MIN_REF_DB;

      for (i = 0; i < agc->mag_history_size; ++i) {
        if (agc->peak < agc->mag_history[i])
          agc->peak = agc->mag_history[i];
      }
    }

    /* Update levels for fast averager */
    peak_delta = agc->peak - agc->fast_level;
    if (peak_delta > 0)
      agc->fast_level += agc->fast_alpha_rise * peak_delta;
    else
      agc->fast_level += agc->fast_alpha_fall * peak_delta;

    /* Update levels for slow averager */
    peak_delta = agc->peak - agc->slow_level;
    if (peak_delta > 0) {
      agc->slow_level += agc->slow_alpha_rise * peak_delta;
      agc->hang_n = 0;
    } else if (agc->hang_n >= agc->hang_max)
      agc->slow_level += agc->slow_alpha_fall * peak_delta;
    else
      ++agc->hang_n;

    /* Keep biggest magnitude */
    x_dBFS_delayed = SU_MAX(agc->fast_level, agc->slow_level);

    /* Is AGC on? */
    if (x_dBFS_delayed < agc->knee)
      x_delayed *= agc->fixed_gain;
    else
      x_delayed *= SU_MAG_RAW(x_dBFS_delayed * (agc->gain_slope - 1));

    x_delayed *= SU_AGC_RESCALE;
  }

  return x_delayed;
}
