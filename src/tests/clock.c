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
#include <sigutils/clock.h>
#include <sigutils/sigutils.h>

#include "test_list.h"
#include "test_param.h"

SUPRIVATE SUBOOL
__su_test_clock_recovery(su_test_context_t *ctx, SUBOOL noisy)
{
  SUBOOL ok = SU_FALSE;
  SUCOMPLEX *input = NULL;
  SUCOMPLEX *carrier = NULL;
  SUFLOAT *omgerr = NULL;
  SUCOMPLEX *output = NULL;
  SUCOMPLEX *rx = NULL;
  SUFLOAT *lock = NULL;
  SUCOMPLEX *baud = NULL;
  SUFLOAT N0 = 0;
  SUCOMPLEX *tx = NULL;
  SUCOMPLEX *data = NULL;
  SUCOMPLEX bbs = 1;
  SUCOMPLEX symsamp = 0;
  SUFLOAT mean_baud = 0;
  SUCOMPLEX symbols[] = {1, I, -1, -I};
  SUCOMPLEX phi0 = 1;

  unsigned int filter_period;
  unsigned int symbol_period;
  unsigned int sync_period;
  unsigned int message;
  unsigned int msgbuf;

  unsigned int rx_delay;
  unsigned int rx_buf = 0;

  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  su_costas_t costas = su_costas_INITIALIZER;
  su_iir_filt_t mf = su_iir_filt_INITIALIZER;
  su_clock_detector_t cd = su_clock_detector_INITIALIZER;
  unsigned int p = 0;
  unsigned int t = 0;
  unsigned int sym;
  unsigned int n = 0;
  unsigned int rx_count = 0;
  unsigned int rx_size;
  int permutations = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize some parameters */
  symbol_period = SU_TEST_COSTAS_SYMBOL_PERIOD;
  filter_period = SU_TEST_MF_SYMBOL_SPAN * symbol_period; /* Span: 6 symbols */
  sync_period   = 1 * 4096; /* Number of samples to allow loop to synchronize */
  message       = 0x414c4f48; /* Some greeting message */
  rx_delay      = filter_period + sync_period - symbol_period / 2;
  rx_size       = SU_CEIL((SUFLOAT) (ctx->params->buffer_size - rx_delay)
                          / symbol_period);

  if (noisy)
    N0          = SU_MAG_RAW(-56) * symbol_period * 4;
  else
    N0          = SU_MAG_RAW(-70) * symbol_period * 4;

  phi0          = SU_C_EXP(I * M_PI / 4); /* Phase offset */

  /* Initialize buffers */
  SU_TEST_ASSERT(input   = su_test_ctx_getc(ctx, "x"));
  SU_TEST_ASSERT(carrier = su_test_ctx_getc(ctx, "carrier"));
  SU_TEST_ASSERT(omgerr  = su_test_ctx_getf(ctx, "oe"));
  SU_TEST_ASSERT(output  = su_test_ctx_getc(ctx, "y"));
  SU_TEST_ASSERT(lock    = su_test_ctx_getf(ctx, "lock"));
  SU_TEST_ASSERT(rx      = su_test_ctx_getc_w_size(ctx, "rx", rx_size));
  SU_TEST_ASSERT(baud    = su_test_ctx_getc(ctx, "baud"));
  SU_TEST_ASSERT(data    = su_test_ctx_getc(ctx, "data"));
  SU_TEST_ASSERT(tx      = su_test_ctx_getc(ctx, "tx"));

  /*
   * In noisy test we assume we are on lock, we just want to retrieve
   * phase offsets from the loop
   */
  SU_TEST_ASSERT(su_costas_init(
      &costas,
      SU_COSTAS_KIND_QPSK,
      noisy ?
          SU_TEST_COSTAS_SIGNAL_FREQ :
          SU_TEST_COSTAS_SIGNAL_FREQ + .05 * SU_TEST_COSTAS_BANDWIDTH,
      6 * SU_TEST_COSTAS_BANDWIDTH,
      3,
      2e-1 * SU_TEST_COSTAS_BANDWIDTH));

  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_INFO("Initial frequency error: %lg\n", costas.ncqo.fnor - ncqo.fnor);

#ifndef SU_TEST_COSTAS_USE_RRC
  SU_TEST_ASSERT(
      su_iir_rrc_init(
          &mf,
          filter_period,
          symbol_period,
          1));
#else
  SU_TEST_ASSERT(
      su_iir_brickwall_init(
          &mf,
          filter_period,
          SU_TEST_COSTAS_BANDWIDTH));
#endif

  /* Send data */
  msgbuf = message;
  SU_INFO("Modulating 0x%x in QPSK...\n", msgbuf);
  SU_INFO("  SNR: %lg dBFS\n", -SU_DB_RAW(N0 / (symbol_period * 4)));
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    if (p >= sync_period) {
      if (p % symbol_period == 0) {
          if (n == 32)
            n = 0;
          msgbuf = message >> n;
          sym = msgbuf & 3;
          n += 2;
          bbs = symbol_period * symbols[sym];
        } else {
          bbs = 0;
        }
      } else {
        /* Send first symbol to synchronize */
        bbs = symbols[1];
      }

    data[p] = bbs;
    input[p] = su_iir_filt_feed(&mf, data[p]);
    tx[p] = phi0 * input[p] * su_ncqo_read(&ncqo) + N0 * su_c_awgn();
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  SU_TEST_ASSERT(
      su_clock_detector_init(
          &cd,
          1,
          1.2 / (SU_TEST_COSTAS_SYMBOL_PERIOD),
          15));

  SU_INFO("Symbol period hint: %lg\n", 1. / cd.bnor);

  /* Feed the loop and perform demodulation */
  for (p = 0; p < ctx->params->buffer_size; ++p) {
    (void) su_ncqo_step(&ncqo);
    su_costas_feed(&costas,  tx[p]);
    carrier[p] = su_ncqo_get(&costas.ncqo);
    output[p]  = su_iir_filt_feed(&mf, costas.y);
    lock[p]    = costas.lock;
    omgerr[p]  = costas.ncqo.fnor - ncqo.fnor;
    baud[p]    = cd.e + I * cd.bnor;

    if (p >= rx_delay) {
      /* Send sample to clock detector */
      su_clock_detector_feed(&cd, output[p]);

      /* Retrieve symbol */
      if (su_clock_detector_read(&cd, &symsamp, 1) == 1) {
        if (p > rx_delay * 4 && rx_count < rx_size) {
          mean_baud += cd.bnor;
          rx[rx_count++] = symsamp;
        }
      }
    }
  }

  SU_INFO("Final frequency error: %lg\n", omgerr[p - 1]);
  SU_INFO("Corrected symbol period: %lg\n", rx_count / mean_baud);

  SU_TEST_ASSERT(SU_ABS(symbol_period - rx_count / mean_baud) < 2);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (ctx->params->dump_fmt) {
    if (mf.x_size > 0)
      ok = ok && su_test_ctx_dumpf(ctx, "mf", mf.b, mf.x_size);
    if (costas.af.x_size > 0)
      ok = ok && su_test_ctx_dumpf(ctx, "b", costas.af.b, costas.af.x_size);
    if (costas.af.y_size > 0)
      ok = ok && su_test_ctx_dumpf(ctx, "a", costas.af.a, costas.af.y_size);
  }

  su_costas_finalize(&costas);
  su_iir_filt_finalize(&mf);
  su_clock_detector_finalize(&cd);

  return ok;
}

#undef __CURRFUNC__

SUBOOL
su_test_clock_recovery(su_test_context_t *ctx)
{
  __su_test_clock_recovery(ctx, SU_FALSE);
}

SUBOOL
su_test_clock_recovery_noisy(su_test_context_t *ctx)
{
  __su_test_clock_recovery(ctx, SU_TRUE);
}
