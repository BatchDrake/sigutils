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

#define SU_TEST_COSTAS_SYMBOL_PERIOD 0x200
#define SU_TEST_COSTAS_SIGNAL_FREQ 1e-4
#define SU_TEST_COSTAS_BANDWIDTH (1. / (2 * SU_TEST_COSTAS_SYMBOL_PERIOD))

SUBOOL
su_test_costas_lock(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUCOMPLEX *input = NULL;
  SUFLOAT *omgerr = NULL;
  SUFLOAT *phierr = NULL;
  SUFLOAT *lock = NULL;
  SUFLOAT N0;
  SUFLOAT t;
  SUFLOAT Ef = 0;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  su_costas_t costas = su_costas_INITIALIZER;
  unsigned int p = 0;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize */
  SU_TEST_ASSERT(input  = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(omgerr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(phierr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(lock   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));

  N0 = 5e-1;

  SU_TEST_ASSERT(su_costas_init(
      &costas,
      SU_COSTAS_KIND_BPSK,
      0, /* fhint */
      1,
      1, /* Disable arm filter */
      1e-3));


  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  /* Build noisy signal */
  SU_INFO("Transmitting a noisy %lg hcps signal...\n", ncqo.fnor);
  SU_INFO("  AWGN amplitude: %lg dBFS\n", SU_DB_RAW(N0));
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    input[p] = su_ncqo_read(&ncqo) + N0 * su_c_awgn();
  }

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_complex_buffer_dump_matlab(
            input,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "costas_input.m",
            "x"));
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  /* Feed the PLL and save phase value */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    (void) su_ncqo_read(&ncqo); /* Used to compute phase errors */
    su_costas_feed(&costas, input[p]);
    input[p]  = su_ncqo_get(&costas.ncqo);
    phierr[p] = su_ncqo_get_phase(&costas.ncqo) - su_ncqo_get_phase(&ncqo);
    lock[p]   = costas.lock;

    if (phierr[p] < 0 || phierr[p] > 2 * PI) {
      phierr[p] -= 2 * PI * floor(phierr[p] / (2 * PI));
      if (phierr[p] > PI)
        phierr[p] -= 2 * PI;
    }
    omgerr[p] = costas.ncqo.fnor - ncqo.fnor;
    Ef += 1. / (.25 * SU_TEST_SIGNAL_BUFFER_SIZE) * (costas.ncqo.fnor - Ef);
  }

  SU_INFO("  Original frequency: %lg\n", ncqo.fnor);
  SU_INFO("  Discovered frequency: %lg\n", Ef);
  SU_INFO("  Relative frequency error: %lg\n", (Ef - ncqo.fnor) / ncqo.fnor);

  /* Ensure we are not skipping frequency */
  SU_TEST_ASSERT(Ef < ncqo.fnor);

  /* We can settle for a relativa frequency offset of 1e-1 */
  SU_TEST_ASSERT(SU_ABS((Ef - ncqo.fnor) / ncqo.fnor) < 1e-1);

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
              "costas_output.m",
              "y"));
    }

    free(input);
  }

  if (phierr != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_buffer_dump_matlab(
              phierr,
              SU_TEST_SIGNAL_BUFFER_SIZE,
              "costas_phierr.m",
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
              "costas_omgerr.m",
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
              "costas_lock.m",
              "lock"));
    }

    free(lock);
  }

  return ok;
}

SUBOOL
su_test_costas_bpsk(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUCOMPLEX *input = NULL;
  SUFLOAT *omgerr = NULL;
  SUCOMPLEX *phierr = NULL;
  SUFLOAT *lock = NULL;
  SUCOMPLEX *rx = NULL;
  SUCOMPLEX x = 0;
  SUCOMPLEX bbs = 1;
  SUFLOAT N0 = 0;
  SUCOMPLEX symbols[] = { /* Out of phase BPSK signal */
      SU_SQRT(2) + I * SU_SQRT(2),
      -SU_SQRT(2) - I * SU_SQRT(2)};
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
  unsigned int p = 0;
  unsigned int t = 0;
  unsigned int bit;
  unsigned int rx_count = 0;
  unsigned int rx_size;

  SU_TEST_START_TICKLESS(ctx);

  /* Initialize some parameters */
  symbol_period = SU_TEST_COSTAS_SYMBOL_PERIOD;
  filter_period = 4 * symbol_period;
  sync_period   = 4096; /* Number of samples to allow loop to synchronize */
  message       = 0x414c4f48; /* Some greeting message */
  rx_delay      = filter_period / 2 + sync_period;
  rx_size       = (SU_TEST_SIGNAL_BUFFER_SIZE - rx_delay) / symbol_period;
  N0            = 5e-1; /* Noise amplitude */

  /* Initialize buffers */
  SU_TEST_ASSERT(input  = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(omgerr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(phierr = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(lock   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(rx     = su_test_complex_buffer_new(rx_size));

  SU_TEST_ASSERT(su_costas_init(
      &costas,
      SU_COSTAS_KIND_BPSK,
      0,
      SU_TEST_COSTAS_BANDWIDTH,
      10,
      2e-1 * SU_TEST_COSTAS_BANDWIDTH));

  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  /* Create Root-Raised-Cosine filter. We will use this to reduce ISI */
  SU_TEST_ASSERT(
      su_iir_rrc_init(
          &mf,
          filter_period,
          symbol_period,
          0));

  /* Send data */
  msgbuf = message;
  SU_INFO("Modulating 0x%x in BPSK...\n", msgbuf);
  SU_INFO("  AWGN amplitude: %lg dBFS\n", SU_DB_RAW(N0));

  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    if (p >= sync_period) {
      if (p >= sync_period && p % symbol_period == 0) {
          bit = msgbuf & 1;
          msgbuf >>= 1;
          bbs = symbol_period * symbols[bit];
        } else {
          bbs = 0;
        }
      } else {
        bbs = symbols[1];
      }

    x = su_iir_filt_feed(&mf, bbs);

    input[p] = x * su_ncqo_read(&ncqo) + N0 * su_c_awgn();
  }

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_complex_buffer_dump_matlab(
            input,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "bpsk_input.m",
            "x"));
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  /* Feed the loop and perform demodulation */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    (void) su_ncqo_step(&ncqo);
    su_costas_feed(&costas, input[p]);
    input[p]  = su_ncqo_get(&costas.ncqo);
    phierr[p] = costas.y;
    lock[p]   = costas.lock;
    omgerr[p] = costas.ncqo.fnor - ncqo.fnor;

    if (p % symbol_period == 0) {
      if (p >= rx_delay) {
        t = (p - rx_delay) / symbol_period;
        bit = SU_C_ARG(phierr[p]) > 0;

        if (t < 32) {
          rx_buf |= bit << t;
        }

        rx[rx_count++] = phierr[p];
      }
    }
  }

  SU_INFO(
      "RX: 0x%x = ~0x%x in %d samples\n",
      rx_buf,
      ~rx_buf,
      SU_TEST_SIGNAL_BUFFER_SIZE);

  SU_TEST_ASSERT(rx_buf == message || rx_buf == ~message);

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
              "bpsk_output.m",
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
              "bpsk_phierr.m",
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
              "bpsk_omgerr.m",
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
              "bpsk_lock.m",
              "lock"));
    }

    free(lock);
  }

  if (rx != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_complex_buffer_dump_matlab(
              rx,
              rx_size,
              "bpsk_rx.m",
              "rx"));
    }

    free(rx);
  }

  if (mf.x_size > 0) {
    if (ctx->dump_results) {
      su_test_buffer_dump_matlab(
          mf.b,
          mf.x_size,
          "bpsk_rrc.m",
          "rrc");
    }
  }

  su_iir_filt_finalize(&mf);

  return ok;
}

SUPRIVATE int
su_test_costas_qpsk_decision(SUCOMPLEX x)
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

SUINLINE void
su_swap(char *a, char *b)
{
  char tmp;

  tmp = *a;
  *a = *b;
  *b = tmp;
}

SUPRIVATE int
su_test_rotcompare(uint32_t original, uint32_t recv)
{
  unsigned char msgsym[16];
  unsigned char map[4] = {0, 1, 2, 3};
  unsigned char tmp;
  unsigned int n;
  unsigned int i, j, k;
  int count = 0;
  for (n = 0; n < 32; n += 2) {
    msgsym[n >> 1] = (original >> n) & 3;
  }

  /*
   * There are 24 permutations of 4 elements. I'm not making this
   * general, I'll settle for several nested loops
   */

  for (i = 0; i < 4; ++i) {
    su_swap(map + i, map + 3);

    for (j = 0; j < 3; ++j) {
      su_swap(map + j, map + 2);

      for (k = 0; k < 2; ++k) {
        su_swap(map + k, map + 1);

        for (n = 0; n < 32; n += 2) {
          if (msgsym[n >> 1] != map[(recv >> n) & 3])
            break;
        }

        if (n == 32)
          return count;

        ++count;

        su_swap(map + k, map + 1);
      }
      su_swap(map + j, map + 2);
    }
    su_swap(map + i, map + 3);
  }

  return -1;
}

#define SU_QPSK_ROT_1 0x01010101
#define SU_QPSK_ROT_2 0x10101010
#define SU_QPSK_ROT_3 (SU_QPSK_ROT_1 | SU_QPSK_ROT_2)

SUBOOL
su_test_costas_qpsk(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUCOMPLEX *input = NULL;
  SUFLOAT *omgerr = NULL;
  SUCOMPLEX *phierr = NULL;
  SUCOMPLEX *rx = NULL;
  SUFLOAT *lock = NULL;
  SUFLOAT N0 = 0;
  SUCOMPLEX x = 0;
  SUCOMPLEX bbs = 1;
  SUCOMPLEX symbols[] = { /* Out of phase BPSK signal */
      SU_SQRT(2) + I * SU_SQRT(2),
      -SU_SQRT(2) - I * SU_SQRT(2),
      SU_SQRT(2) - I * SU_SQRT(2),
      -SU_SQRT(2) + I * SU_SQRT(2),
  };
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
  sync_period   = 2 * 4096; /* Number of samples to allow loop to synchronize */
  message       = 0x414c4f48; /* Some greeting message */
  rx_delay      = filter_period / 2 + sync_period - symbol_period / 2;
  rx_size       = (SU_TEST_SIGNAL_BUFFER_SIZE - rx_delay) / symbol_period;
  N0            = 4e-1; /* Noise amplitude */

  /* Initialize buffers */
  SU_TEST_ASSERT(input  = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(omgerr = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(phierr = su_test_complex_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(lock   = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));
  SU_TEST_ASSERT(rx     = su_test_complex_buffer_new(rx_size));

  SU_TEST_ASSERT(su_costas_init(
      &costas,
      SU_COSTAS_KIND_QPSK,
      0,
      SU_TEST_COSTAS_BANDWIDTH,
      10,
      2e-1 * SU_TEST_COSTAS_BANDWIDTH));

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            costas.af.b,
            costas.af.x_size,
            "qpsk_af.m",
            "af"));
  }

  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  /* Create Root-Raised-Cosine filter. We will use this to reduce ISI */
  SU_TEST_ASSERT(
      su_iir_rrc_init(
          &mf,
          filter_period,
          symbol_period,
          0));

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            mf.b,
            mf.x_size,
            "qpsk_mf.m",
            "mf"));
  }

  /* Send data */
  msgbuf = message;
  SU_INFO("Modulating 0x%x in QPSK...\n", msgbuf);
  SU_INFO("  AWGN amplitude: %lg dBFS\n", SU_DB_RAW(N0));
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    if (p >= sync_period) {
      if (p >= sync_period && p % symbol_period == 0) {
          if (n == 32) {
            n = 0;
          }
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

    x = su_iir_filt_feed(&mf, bbs);

    input[p] = x * su_ncqo_read(&ncqo) + N0 * su_c_awgn();
  }

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_complex_buffer_dump_matlab(
            input,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "qpsk_input.m",
            "x"));
  }

  /* Restart NCQO */
  su_ncqo_init(&ncqo, SU_TEST_COSTAS_SIGNAL_FREQ);

  SU_TEST_TICK(ctx);

  /* Feed the loop and perform demodulation */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    (void) su_ncqo_step(&ncqo);
    su_costas_feed(&costas, input[p]);
    input[p]  = su_ncqo_get(&costas.ncqo);
    phierr[p] = costas.y;
    lock[p]   = costas.lock;
    omgerr[p] = costas.ncqo.fnor - ncqo.fnor;

    if (p % symbol_period == 0) {
      if (p >= rx_delay) {
        t = (p - rx_delay) / symbol_period;
        sym = su_test_costas_qpsk_decision(phierr[p]);
        if (t < 32) {
          rx_buf |= sym << (2 * t);
        }

        rx[rx_count++] = phierr[p];
      }
    }
  }

  permutations = su_test_rotcompare(rx_buf, message);

  SU_INFO(
      "RX: 0x%x in %d samples\n",
      rx_buf,
      SU_TEST_SIGNAL_BUFFER_SIZE);
  SU_TEST_ASSERT(permutations != -1);
  SU_INFO(
        "RX: message decoded after %d permutations\n",
        permutations);
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
              "qpsk_output.m",
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
              "qpsk_phierr.m",
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
              "qpsk_omgerr.m",
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
              "qpsk_lock.m",
              "lock"));
    }

    free(lock);
  }

  if (rx != NULL) {
    if (ctx->dump_results) {
      SU_TEST_ASSERT(
          su_test_complex_buffer_dump_matlab(
              rx,
              rx_size,
              "qpsk_rx.m",
              "rx"));
    }

    free(rx);
  }

  if (mf.x_size > 0) {
    if (ctx->dump_results) {
      su_test_buffer_dump_matlab(
          mf.b,
          mf.x_size,
          "qpsk_rrc.m",
          "rrc");
    }
  }

  su_iir_filt_finalize(&mf);

  return ok;
}
