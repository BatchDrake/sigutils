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

#include "test_param.h"
#include "test_list.h"

SUBOOL
su_test_butterworth_lpf(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *hi = NULL;
  SUFLOAT *lo = NULL;
  unsigned int p = 0;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  su_iir_filt_t lpf = su_iir_filt_INITIALIZER;

  SU_TEST_START_TICKLESS(ctx);

  SU_TEST_ASSERT(hi = su_test_ctx_getf(ctx, "hi"));
  SU_TEST_ASSERT(lo = su_test_ctx_getf(ctx, "lo"));

  SU_TEST_ASSERT(su_iir_bwlpf_init(&lpf, 5, 0.25));

  SU_TEST_TICK(ctx);

  su_ncqo_init(&ncqo, 1);

  for (p = 0; p < ctx->params->buffer_size; ++p)
    hi[p] = SU_C_REAL(su_iir_filt_feed(
        &lpf,
        su_ncqo_read(&ncqo)));

  if (ctx->params->dump_fmt) {
    printf(" hi pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(hi, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" hi mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(hi, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" hi std: " SUFLOAT_FMT "\n", su_test_buffer_std(hi, SU_TEST_SIGNAL_BUFFER_SIZE));
  }

  su_ncqo_set_freq(&ncqo, .125);

  for (p = 0; p < ctx->params->buffer_size; ++p)
    lo[p] = SU_C_REAL(su_iir_filt_feed(
        &lpf,
        su_ncqo_read(&ncqo)));

  if (ctx->params->dump_fmt) {
    printf(" lo pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(lo, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" lo mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(lo, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" lo std: " SUFLOAT_FMT "\n", su_test_buffer_std(lo, SU_TEST_SIGNAL_BUFFER_SIZE));
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_iir_filt_finalize(&lpf);

  return ok;
}


