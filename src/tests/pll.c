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

SUBOOL
su_test_pll(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *input = NULL;
  SUFLOAT *omgerr = NULL;
  SUFLOAT *phierr = NULL;
  SUFLOAT *lock = NULL;

  SUFLOAT t;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  su_pll_t pll = su_pll_INITIALIZER;
  unsigned int p = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize */
  SU_TEST_ASSERT(input  = su_test_ctx_getf(ctx, "x"));
  SU_TEST_ASSERT(omgerr = su_test_ctx_getf(ctx, "oe"));
  SU_TEST_ASSERT(phierr = su_test_ctx_getf(ctx, "pe"));
  SU_TEST_ASSERT(lock   = su_test_ctx_getf(ctx, "lock"));

  SU_TEST_ASSERT(su_pll_init(&pll,
      SU_TEST_PLL_SIGNAL_FREQ * 0.5,
      SU_TEST_PLL_BANDWIDTH));

  su_ncqo_init(&ncqo, SU_TEST_PLL_SIGNAL_FREQ);

  /* Create a falling sinusoid */
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    input[p] = 0 * (0.5 - rand() / (double) RAND_MAX);
    input[p] += su_ncqo_read_i(&ncqo);
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_PLL_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  /* Feed the PLL and save phase value */
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    (void) su_ncqo_read_i(&ncqo); /* Used to compute phase errors */
    su_pll_feed(&pll, input[p]);
    input[p]  = su_ncqo_get_i(&pll.ncqo);
    phierr[p] = su_ncqo_get_phase(&pll.ncqo) - su_ncqo_get_phase(&ncqo);
    lock[p]   = pll.lock;

    if (phierr[p] < 0 || phierr[p] > 2 * PI) {
      phierr[p] -= 2 * PI * floor(phierr[p] / (2 * PI));
      if (phierr[p] > PI)
        phierr[p] -= 2 * PI;
    }
    omgerr[p] = pll.ncqo.omega - ncqo.omega;
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_pll_finalize(&pll);

  return ok;
}


