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


#define SU_TEST_PLL_SIGNAL_FREQ 0.025
#define SU_TEST_PLL_BANDWIDTH   (1e-4)

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
  SU_TEST_ASSERT(input  = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(omgerr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(phierr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(lock   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));

  SU_TEST_ASSERT(su_pll_init(&pll,
      SU_TEST_PLL_SIGNAL_FREQ * 0.5,
      SU_TEST_PLL_BANDWIDTH));

  su_ncqo_init(&ncqo, SU_TEST_PLL_SIGNAL_FREQ);

  /* Create a falling sinusoid */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    input[p] = 0 * (0.5 - rand() / (double) RAND_MAX);
    input[p] += su_ncqo_read_i(&ncqo);
  }

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            input,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "input.m",
            "input"));
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_PLL_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  /* Feed the PLL and save phase value */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
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

  if (input != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              input,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "output.m",
              "output"));
    }

    free(input);
  }

  if (phierr != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              phierr,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "phierr.m",
              "phierr"));
    }

    free(phierr);
  }

  if (omgerr != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              omgerr,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "omgerr.m",
              "omgerr"));
    }

    free(omgerr);
  }

  if (lock != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              lock,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "lock.m",
              "lock"));
    }

    free(lock);
  }

  return ok;
}


