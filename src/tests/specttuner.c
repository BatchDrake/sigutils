/*

  Copyright (C) 2018 Gonzalo José Carracedo Carballal

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sigutils/specttuner.h>
#include <sigutils/ncqo.h>

#include <sigutils/sigutils.h>

#include "test_list.h"
#include "test_param.h"

struct su_specttuner_context {
  SUCOMPLEX *output;
  unsigned int p;
};

SUPRIVATE SUBOOL
su_specttuner_append(
    const su_specttuner_channel_t *channel,
    void *private,
    const SUCOMPLEX *data, /* This pointer remains valid until the next call to feed */
    SUSCOUNT size)
{
  struct su_specttuner_context *ctx = (struct su_specttuner_context *) private;

  memcpy(ctx->output + ctx->p, data, size * sizeof(SUCOMPLEX));
  ctx->p += size;

  SU_INFO("  Append %d samples!\n", size);

  return SU_TRUE;
}

SUBOOL
su_test_ctx_dumpc(
    su_test_context_t *ctx,
    const char *name,
    const SUCOMPLEX *data,
    SUSCOUNT size);

SUBOOL
su_test_specttuner_two_tones(su_test_context_t *ctx)
{
  SUCOMPLEX *input = NULL;
  SUCOMPLEX *output = NULL;
  unsigned int p;
  struct sigutils_specttuner_params st_params =
      sigutils_specttuner_params_INITIALIZER;
  struct sigutils_specttuner_channel_params ch_params =
        sigutils_specttuner_channel_params_INITIALIZER;
  struct su_specttuner_context out_ctx;
  su_specttuner_channel_t *ch = NULL;
  su_specttuner_t *st = NULL;
  su_ncqo_t lo1, lo2;
  SUBOOL ok = SU_FALSE;

  SU_TEST_START_TICKLESS(ctx);

  SU_TEST_ASSERT(input = su_test_ctx_getc(ctx, "x"));
  SU_TEST_ASSERT(output = su_test_ctx_getc(ctx, "y"));

  /* Initialize tuner */
  SU_TEST_ASSERT(st = su_specttuner_new(&st_params));

  /* Initialize oscillators */
  su_ncqo_init_fixed(
      &lo1,
      SU_ABS2NORM_FREQ(SU_TEST_SPECTTUNER_SAMP_RATE, SU_TEST_SPECTTUNER_FREQ1));

  su_ncqo_init_fixed(
      &lo2,
      SU_ABS2NORM_FREQ(SU_TEST_SPECTTUNER_SAMP_RATE, SU_TEST_SPECTTUNER_FREQ2));

  /* Populate buffer */
  SU_INFO(
      "Transmitting two tones at %lg Hz and %lg Hz\n",
      SU_TEST_SPECTTUNER_FREQ1,
      SU_TEST_SPECTTUNER_FREQ2);
  SU_INFO("  AWGN amplitude: %lg dBFS\n", SU_DB_RAW(SU_TEST_SPECTTUNER_N0));

  for (p = 0; p < ctx->params->buffer_size; ++p)
    input[p] = su_ncqo_read(&lo1) + su_ncqo_read(&lo2);

    /*input[p]
          = .25 * (su_ncqo_read(&lo1) + su_ncqo_read(&lo2))
          + SU_TEST_SPECTTUNER_N0 * su_c_awgn();*/

  /* Define channel */
  memset(output, 0, sizeof(SUCOMPLEX) * ctx->params->buffer_size);
  out_ctx.output = output;
  out_ctx.p = 0;

  ch_params.privdata = &out_ctx;
  ch_params.on_data = su_specttuner_append;

  ch_params.bw = SU_NORM2ANG_FREQ(
      SU_ABS2NORM_FREQ(SU_TEST_SPECTTUNER_SAMP_RATE, 100));
  ch_params.f0 = SU_NORM2ANG_FREQ(
      SU_ABS2NORM_FREQ(SU_TEST_SPECTTUNER_SAMP_RATE, SU_TEST_SPECTTUNER_FREQ1));

  SU_TEST_ASSERT(ch = su_specttuner_open_channel(st, &ch_params));

  SU_TEST_TICK(ctx);

  SU_TEST_ASSERT(su_specttuner_feed_bulk(st, input, ctx->params->buffer_size));

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (st != NULL)
    su_specttuner_destroy(st);

  return ok;
}
