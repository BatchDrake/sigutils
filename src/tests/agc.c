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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sigutils/sampling.h>
#include <sigutils/ncqo.h>
#include <sigutils/iir.h>
#include <sigutils/agc.h>
#include <sigutils/pll.h>

#include <sigutils/sigutils.h>

#include "test_list.h"
#include "test_param.h"

SUPRIVATE SUBOOL
su_test_check_peak(
    const su_test_context_t *ctx,
    const SUFLOAT *buffer,
    unsigned int size,
    unsigned int window,
    SUFLOAT peak_low,
    SUFLOAT peak_high)
{
  unsigned int i, j;
  SUFLOAT peak = 0;
  SUBOOL ok = SU_FALSE;

  /* Skip window. AGC needs some time to start */
  for (i = window; i < size - window; ++i) {
    peak = 0;
    for (j = 0; j < window; ++j) {
      if (SU_ABS(buffer[i + j]) > peak)
        peak = SU_ABS(buffer[i + j]);
    }

    SU_TEST_ASSERT(peak >= peak_low);
    SU_TEST_ASSERT(peak <= peak_high);
  }

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_test_agc_transient(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *buffer = NULL;
  SUFLOAT t;
  su_agc_t agc = su_agc_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;

  unsigned int p = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize */
  SU_TEST_ASSERT(buffer = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;

  if (!su_agc_init(&agc, &agc_params))
    goto done;

  /* Create a spike */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    t = p - SU_TEST_SIGNAL_BUFFER_SIZE / 2;
    buffer[p] = 1e-2 * rand() / (double) RAND_MAX;

    buffer[p] += SU_EXP(-t * t);
  }

  if (ctx->params->dump_fmt) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            buffer,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "spike.m",
            "spike"));
  }

  SU_TEST_TICK(ctx);

  /* Feed the AGC and put samples back in buffer */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    buffer[p] = SU_C_REAL(su_agc_feed(&agc, buffer[p]));
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_agc_finalize(&agc);

  if (buffer != NULL) {
    if (ctx->params->dump_fmt) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              buffer,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "corrected.m",
              "corrected"));
    }

    free(buffer);
  }

  return ok;
}

SUBOOL
su_test_agc_steady_rising(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *buffer = NULL;
  SUFLOAT t;
  su_agc_t agc = su_agc_INITIALIZER;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  unsigned int p = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize */
  SU_TEST_ASSERT(buffer = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  SU_TEST_ASSERT(su_agc_init(&agc, &agc_params));

  su_ncqo_init(&ncqo, SU_TEST_AGC_SIGNAL_FREQ);

  /* Create a rising sinusoid */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    t = p - SU_TEST_SIGNAL_BUFFER_SIZE / 2;
    buffer[p] = 1e-2 * rand() / (double) RAND_MAX;

    buffer[p] += 0.2 * floor(1 + 5 * p / SU_TEST_SIGNAL_BUFFER_SIZE) * su_ncqo_read_i(&ncqo);
  }

  if (ctx->params->dump_fmt) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            buffer,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "steady.m",
            "steady"));
  }

  SU_TEST_TICK(ctx);

  /* Feed the AGC and put samples back in buffer */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    buffer[p] = SU_C_REAL(su_agc_feed(&agc, buffer[p]));
  }

  /* TODO: Improve levels */
  SU_TEST_ASSERT(
      su_test_check_peak(
          ctx,
          buffer,
          SU_TEST_SIGNAL_BUFFER_SIZE,
          2 * SU_TEST_AGC_WINDOW,
          SU_AGC_RESCALE * 0.9 ,
          SU_AGC_RESCALE * 1.1));

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_agc_finalize(&agc);

  if (buffer != NULL) {
    if (ctx->params->dump_fmt) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              buffer,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "corrected.m",
              "corrected"));
    }

    free(buffer);
  }

  return ok;
}

SUBOOL
su_test_agc_steady_falling(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *input = NULL;
  SUFLOAT *output = NULL;
  SUFLOAT t;
  su_agc_t agc = su_agc_INITIALIZER;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  unsigned int p = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize */
  SU_TEST_ASSERT(input = su_test_ctx_getf(ctx, "x"));
  SU_TEST_ASSERT(output = su_test_ctx_getf(ctx, "y"));
  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  SU_TEST_ASSERT(su_agc_init(&agc, &agc_params));

  su_ncqo_init(&ncqo, SU_TEST_AGC_SIGNAL_FREQ);

  /* Create a falling sinusoid */
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    t = p - ctx->params->buffer_size / 2;
    input[p] = 1e-2 * rand() / (double) RAND_MAX;

    input[p] += 0.2 * floor(5 - 5 * p / ctx->params->buffer_size) * su_ncqo_read_i(&ncqo);
  }

  SU_TEST_TICK(ctx);

  /* Feed the AGC and put samples back in buffer */
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    output[p] = SU_C_REAL(su_agc_feed(&agc, input[p]));
  }

  /* TODO: Improve levels */
  SU_TEST_ASSERT(
      su_test_check_peak(
          ctx,
          input,
          ctx->params->buffer_size,
          2 * SU_TEST_AGC_WINDOW,
          SU_AGC_RESCALE * 0.9 ,
          SU_AGC_RESCALE * 1.1));

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_agc_finalize(&agc);

  return ok;
}

