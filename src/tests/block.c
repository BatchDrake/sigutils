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

SUBOOL
su_test_block(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX samp = 0;

  SU_TEST_START(ctx);

  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  block = su_block_new("agc", &agc_params);
  SU_TEST_ASSERT(block != NULL);

  /* Plug block to the reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, block, 0));

  /* Try to read (this must fail) */
  SU_TEST_ASSERT(su_block_port_read(&port, &samp, 1) == SU_BLOCK_PORT_READ_ERROR_ACQUIRE);

  ok = SU_TRUE;
done:
  SU_TEST_END(ctx);

  if (su_block_port_is_plugged(&port))
    su_block_port_unplug(&port);

  if (block != NULL)
    su_block_destroy(block);

  return ok;
}

SUBOOL
su_test_block_plugging(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *agc_block = NULL;
  su_block_t *wav_block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX buffer[17]; /* Prime number on purpose */
  SUFLOAT real = 0;
  int i;
  FILE *fp = NULL;
  ssize_t got;

  SU_TEST_START(ctx);

  if (ctx->dump_results) {
    fp = fopen("su_test_block_plugging.raw", "wb");
    SU_TEST_ASSERT(fp != NULL);
  }

  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  agc_block = su_block_new("agc", &agc_params);
  SU_TEST_ASSERT(agc_block != NULL);

  wav_block = su_block_new("wavfile", "test.wav");
  SU_TEST_ASSERT(wav_block != NULL);

  /* Plug wav file to AGC */
  SU_TEST_ASSERT(su_block_plug(wav_block, 0, 0, agc_block));

  /* Plug AGC to the reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, agc_block, 0));

  /* Try to read (this must work) */
  do {
    got = su_block_port_read(&port, buffer, 17);
    SU_TEST_ASSERT(got >= 0);

    if (fp != NULL)
      for (i = 0; i < got; ++i) {
        real = SU_C_REAL(buffer[i]);
        fwrite(&real, sizeof(SUFLOAT), 1, fp);
      }
  } while (got > 0);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (su_block_port_is_plugged(&port))
    su_block_port_unplug(&port);

  if (agc_block != NULL)
    su_block_destroy(agc_block);

  if (wav_block != NULL)
    su_block_destroy(wav_block);

  if (fp != NULL)
    fclose(fp);

  return ok;
}

SUBOOL
su_test_tuner(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *agc_block = NULL;
  su_block_t *tuner_block = NULL;
  su_block_t *wav_block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX buffer[17]; /* Prime number on purpose */
  SUFLOAT samp = 0;
  int i;
  FILE *fp = NULL;
  ssize_t got;

  /* Block properties */
  int *samp_rate;
  SUFLOAT *fc;
  SUFLOAT *T;
  SUFLOAT *beta;
  unsigned int *size;
  const unsigned int *d;

  SU_TEST_START(ctx);

  if (ctx->dump_results) {
    fp = fopen("su_test_tuner.raw", "wb");
    SU_TEST_ASSERT(fp != NULL);
  }

  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  wav_block = su_block_new("wavfile", "test.wav");
  SU_TEST_ASSERT(wav_block != NULL);

  samp_rate = su_block_get_property_ref(
      wav_block,
      SU_BLOCK_PROPERTY_TYPE_INTEGER,
      "samp_rate");
  SU_TEST_ASSERT(samp_rate != NULL);
  SU_TEST_ASSERT(*samp_rate == 8000);

  SU_INFO("Wav file opened, sample rate: %d\n", *samp_rate);

  tuner_block = su_block_new(
      "tuner",
      SU_ABS2NORM_FREQ(*samp_rate, 910),  /* Center frequency (910 Hz) */
      SU_T2N_FLOAT(*samp_rate, 1. / 468), /* Signal is 468 baud */
      (SUFLOAT) 1,                        /* Cast is mandatory */
      1000);                              /* 1000 coefficients */
  SU_TEST_ASSERT(tuner_block != NULL);

  d = su_block_get_property_ref(
      tuner_block,
      SU_BLOCK_PROPERTY_TYPE_INTEGER,
      "decimation");
  SU_TEST_ASSERT(d != NULL);
  SU_INFO("Tuner created, decimation: %d\n", *d);

  agc_block = su_block_new("agc", &agc_params);
  SU_TEST_ASSERT(agc_block != NULL);

  /* Plug wav file to tuner */
  SU_TEST_ASSERT(su_block_plug(wav_block, 0, 0, tuner_block));

  /* Plug tuner to AGC */
  SU_TEST_ASSERT(su_block_plug(tuner_block, 0, 0, agc_block));

  /* Plug AGC to the reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, agc_block, 0));

  /* Try to read (this must work) */
  do {
    got = su_block_port_read(&port, buffer, 17);
    SU_TEST_ASSERT(got >= 0);

    if (fp != NULL)
      for (i = 0; i < got; ++i) {
        samp = SU_C_REAL(buffer[i]);
        fwrite(&samp, sizeof(SUFLOAT), 1, fp);
        samp = SU_C_IMAG(buffer[i]);
        fwrite(&samp, sizeof(SUFLOAT), 1, fp);
      }
  } while (got > 0);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (su_block_port_is_plugged(&port))
    su_block_port_unplug(&port);

  if (agc_block != NULL)
    su_block_destroy(agc_block);

  if (tuner_block != NULL)
    su_block_destroy(tuner_block);

  if (wav_block != NULL)
    su_block_destroy(wav_block);

  if (fp != NULL)
    fclose(fp);

  return ok;
}

SUBOOL
su_test_costas_block(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *costas_block = NULL;
  su_block_t *agc_block = NULL;
  su_block_t *wav_block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX buffer[17]; /* Prime number on purpose */
  SUFLOAT samp = 0;
  int i;
  unsigned int j = 0;
  FILE *fp = NULL;
  ssize_t got;

  /* Signal properties */
  const SUFLOAT baud = 468;
  const SUFLOAT arm_bw = .5 * baud;
  const unsigned int arm_order = 10;
  const SUFLOAT loop_bw = 1e-1 * baud;
  const unsigned int sample_count = 8000 * 59;
  /* Block properties */
  int *samp_rate;
  SUFLOAT *f;

  unsigned int *size;

  SU_TEST_START(ctx);

  if (ctx->dump_results) {
    fp = fopen("su_test_costas_block.raw", "wb");
    SU_TEST_ASSERT(fp != NULL);
  }

  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t      = 20;
  agc_params.slow_fall_t      = 40;

  agc_params.threshold        = SU_DB(2e-2);

  agc_params.hang_max         = 30;
  agc_params.slope_factor     = 0;

  wav_block = su_block_new("wavfile", "test.wav");
  SU_TEST_ASSERT(wav_block != NULL);

  samp_rate = su_block_get_property_ref(
      wav_block,
      SU_BLOCK_PROPERTY_TYPE_INTEGER,
      "samp_rate");
  SU_TEST_ASSERT(samp_rate != NULL);
  SU_TEST_ASSERT(*samp_rate == 8000);

  SU_INFO("Wav file opened, sample rate: %d\n", *samp_rate);

  agc_block = su_block_new("agc", &agc_params);
  SU_TEST_ASSERT(agc_block != NULL);

  costas_block = su_block_new(
      "costas",
      SU_COSTAS_KIND_QPSK,
      SU_ABS2NORM_FREQ(*samp_rate, 900),
      SU_ABS2NORM_FREQ(*samp_rate, arm_bw),
      arm_order,
      SU_ABS2NORM_FREQ(*samp_rate, loop_bw));
  SU_TEST_ASSERT(costas_block != NULL);

  f = su_block_get_property_ref(
      costas_block,
      SU_BLOCK_PROPERTY_TYPE_FLOAT,
      "f");
  SU_TEST_ASSERT(f != NULL);
  SU_INFO(
      "Costas loop created, initial frequency: %lg Hz\n",
      SU_NORM2ABS_FREQ(*samp_rate, *f));

  /* Plug wav file directly to AGC (there should be a tuner before this) */
  SU_TEST_ASSERT(su_block_plug(wav_block, 0, 0, agc_block));

  /* Plug AGC to Costas loop */
  SU_TEST_ASSERT(su_block_plug(agc_block, 0, 0, costas_block));

  /* Plug Costas loop to the reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, costas_block, 0));

  /* Try to read (this must work) */
  while (j < sample_count) {
    got = su_block_port_read(&port, buffer, 17);
    SU_TEST_ASSERT(got >= 0);

    if (fp != NULL)
      for (i = 0; i < got; ++i) {
        samp = SU_C_REAL(buffer[i]);
        fwrite(&samp, sizeof(SUFLOAT), 1, fp);
        samp = SU_C_IMAG(buffer[i]);
        fwrite(&samp, sizeof(SUFLOAT), 1, fp);
      }

    if ((j % (17 * 25)) == 0)
      SU_INFO("Center frequency: %lg Hz\r", SU_NORM2ABS_FREQ(*samp_rate, *f));
    j += got;
  }

  SU_INFO("\n");
  SU_TEST_ASSERT(SU_NORM2ABS_FREQ(*samp_rate, *f) > 909 &&
                 SU_NORM2ABS_FREQ(*samp_rate, *f) < 911);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (su_block_port_is_plugged(&port))
    su_block_port_unplug(&port);

  if (costas_block != NULL)
    su_block_destroy(costas_block);

  if (agc_block != NULL)
    su_block_destroy(agc_block);

  if (wav_block != NULL)
    su_block_destroy(wav_block);

  if (fp != NULL)
    fclose(fp);

  return ok;
}

