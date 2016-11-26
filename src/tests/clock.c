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

#undef __CURRFUNC__
#define __CURRFUNC__ __caller

SUPRIVATE int
su_test_clock_qpsk_decision(SUCOMPLEX x)
{
  SUFLOAT a = SU_C_ARG(x);

  if (a < -M_PI / 2)
    return 0;
  else if (a < 0)
    return 1;
  else if (a < M_PI / 2)
    return 2;
  else
    return 3;
}

SUPRIVATE SUBOOL
__su_test_clock_recovery(
    su_test_context_t *ctx,
    const char *__caller,
    SUBOOL noisy)
{
  SUBOOL ok = SU_FALSE;
  SUCOMPLEX *input = NULL;
  SUFLOAT *omgerr = NULL;
  SUCOMPLEX *phierr = NULL;
  SUCOMPLEX *rx = NULL;
  SUFLOAT *lock = NULL;
  SUFLOAT *baud = NULL;
  SUFLOAT N0 = 0;
  SUCOMPLEX x = 0;
  SUCOMPLEX bbs = 1;
  SUCOMPLEX symsamp = 0;
  SUFLOAT mean_baud = 0;
  SUCOMPLEX symbols[] = {1, I, -1, -I};

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
  filter_period = 4 * symbol_period;
  sync_period   = 1 * 4096; /* Number of samples to allow loop to synchronize */
  message       = 0x414c4f48; /* Some greeting message */
  rx_delay      = filter_period + sync_period - symbol_period / 2;
  rx_size       = (SU_TEST_SIGNAL_BUFFER_SIZE - rx_delay) / symbol_period;

  if (noisy)
    N0          = SU_MAG_RAW(2);
  else
    N0          = SU_MAG_RAW(-10);

  /* Initialize buffers */
  SU_TEST_ASSERT(input  = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(omgerr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(phierr = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(lock   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(rx     = su_test_complex_buffer_new(rx_size));
  SU_TEST_ASSERT(baud   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  /*
   * In noisy test we assume we are on lock, we just want to retrieve
   * phase offsets from the loop
   */
  SU_TEST_ASSERT(su_costas_init(
      &costas,
      SU_COSTAS_KIND_QPSK,
      noisy ? SU_TEST_COSTAS_SIGNAL_FREQ : 0,
      SU_TEST_COSTAS_BANDWIDTH,
      10,
      2e-1 * SU_TEST_COSTAS_BANDWIDTH));

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            costas.af.b,
            costas.af.x_size,
            "clock_af.m",
            "af"));
  }

  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

#ifndef SU_TEST_COSTAS_USE_RRC
  SU_TEST_ASSERT(
      su_iir_rrc_init(
          &mf,
          filter_period,
          .5 * symbol_period,
          0.35));
#else
  SU_TEST_ASSERT(
      su_iir_brickwall_init(
          &mf,
          filter_period,
          1. / symbol_period));
#endif

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            mf.b,
            mf.x_size,
            "clock_mf.m",
            "mf"));
  }

  /* Send data */
  msgbuf = message;
  SU_INFO("Modulating 0x%x in QPSK...\n", msgbuf);
  SU_INFO("  SNR: %lg dBFS\n", -SU_DB_RAW(N0));
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
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

    x = su_iir_filt_feed(&mf, .5 * bbs);

    input[p] = x * su_ncqo_read(&ncqo) + N0 * su_c_awgn();
  }

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_complex_buffer_dump_matlab(
            input,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "clock_input.m",
            "x"));
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  SU_TEST_ASSERT(
      su_clock_detector_init(
          &cd,
          .707,
          1.13 / (SU_TEST_COSTAS_SYMBOL_PERIOD),
          15));

  SU_INFO("Symbol period hint: %lg\n", 1. / cd.bnor);

  /* Feed the loop and perform demodulation */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    (void) su_ncqo_step(&ncqo);
    su_costas_feed(&costas, input[p]);
    input[p]  = su_ncqo_get(&costas.ncqo);
    phierr[p] = su_iir_filt_feed(&mf, costas.y);
    lock[p]   = costas.lock;
    omgerr[p] = costas.ncqo.fnor - ncqo.fnor;
    baud[p]   = cd.bnor;

    if (p >= rx_delay) {
      /* Send sample to clock detector */
      su_clock_detector_feed(&cd, SU_SQRT(2) * phierr[p]);

      /* Retrieve symbol */
      if (su_clock_detector_read(&cd, &symsamp, 1) == 1) {
        /* t = (p - rx_delay) / symbol_period; */
        /* sym = su_test_clock_qpsk_decision(symsamp[p]); */
        if (p > rx_delay * 5 && rx_count < rx_size) {
          mean_baud += cd.bnor;

          rx[rx_count++] = symsamp;
        }
      }
    }
  }

  SU_INFO("Corrected symbol period: %lg\n", rx_count / mean_baud);

  SU_TEST_ASSERT(SU_ABS(symbol_period - rx_count / mean_baud) < 1);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_costas_finalize(&costas);

  if (input != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_complex_buffer_dump_matlab(
              input,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "clock_output.m",
              "y"));
    }

    free(input);
  }

  if (phierr != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_complex_buffer_dump_matlab(
              phierr,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "clock_phierr.m",
              "pe"));
    }

    free(phierr);
  }

  if (omgerr != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              omgerr,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "clock_omgerr.m",
              "oe"));
    }

    free(omgerr);
  }

  if (lock != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              lock,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "clock_lock.m",
              "lock"));
    }

    free(lock);
  }

  if (rx != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_complex_buffer_dump_matlab(
              rx,
              rx_count,
              "clock_rx.m",
              "rx"));
    }

    free(rx);
  }

  if (rx != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              baud,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "clock_baud.m",
              "baud"));
    }

    free(baud);
  }

  if (mf.x_size > 0) {
    if (ctx->dump_results) {
      su_test_buffer_dump_matlab(
          mf.b,
          mf.x_size,
          "clock_rrc.m",
          "rrc");
    }
  }

  su_iir_filt_finalize(&mf);
  su_clock_detector_finalize(&cd);

  return ok;
}

#undef __CURRFUNC__

SUBOOL
su_test_clock_recovery(su_test_context_t *ctx)
{
  __su_test_clock_recovery(ctx, __FUNCTION__, SU_FALSE);
}

SUBOOL
su_test_clock_recovery_noisy(su_test_context_t *ctx)
{
  __su_test_clock_recovery(ctx, __FUNCTION__, SU_TRUE);
}
