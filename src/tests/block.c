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
  SUCOMPLEX *rx = NULL;
  SUFLOAT real = 0;
  int i;
  unsigned int j = 0;
  ssize_t got;

  SU_TEST_START(ctx);

  SU_TEST_ASSERT(rx = su_test_ctx_getc(ctx, "rx"));

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

    if (ctx->params->dump_fmt && j + got <= ctx->params->buffer_size)
      for (i = 0; i < got; ++i)
        rx[i + j] = buffer[i];

    j += got;
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
  SUCOMPLEX *rx = NULL;
  SUFLOAT samp = 0;
  int i;
  unsigned int j = 0;
  ssize_t got;

  /* Block properties */
  int *samp_rate;
  SU_TEST_START(ctx);

  SU_TEST_ASSERT(rx = su_test_ctx_getc(ctx, "rx"));

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
      SU_ABS2NORM_FREQ(*samp_rate, 468),  /* Signal is 468 baud */
      SU_ABS2NORM_FREQ(*samp_rate, 2000), /* Move signal to 2 KHz */
      500);                               /* 500 coefficients */
  SU_TEST_ASSERT(tuner_block != NULL);

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

    if (ctx->params->dump_fmt && j + got <= ctx->params->buffer_size)
      for (i = 0; i < got; ++i)
        rx[i + j] = buffer[i];

    j += got;
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
  SUCOMPLEX *rx = NULL;
  SUFLOAT *freq = NULL;
  SUFLOAT samp = 0;
  int i;
  unsigned int j = 0;
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

  SU_TEST_ASSERT(freq = su_test_ctx_getf_w_size(ctx, "freq", sample_count));
  SU_TEST_ASSERT(rx = su_test_ctx_getc_w_size(ctx, "rx", sample_count));

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
    got = su_block_port_read(&port, buffer, 1);
    SU_TEST_ASSERT(got >= 0);

    if (ctx->params->dump_fmt)
      for (i = 0; i < got; ++i) {
        freq[i + j] = *f;
        rx[i + j] = buffer[i];
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

  return ok;
}

SUBOOL
su_test_rrc_block(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *costas_block = NULL;
  su_block_t *agc_block = NULL;
  su_block_t *rrc_block = NULL;
  su_block_t *wav_block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX buffer[17]; /* Prime number on purpose */
  SUCOMPLEX *rx = NULL;
  SUFLOAT *freq = NULL;
  SUFLOAT samp = 0;
  int i;
  unsigned int j = 0;
  ssize_t got;

  /* Signal properties */
  const SUFLOAT baud = 468;
  const SUFLOAT arm_bw = 2 * baud;
  const unsigned int arm_order = 3;
  const SUFLOAT loop_bw = 1e-1 * baud;
  const unsigned int sample_count = 8000 * 59;
  /* Block properties */
  int *samp_rate;
  SUFLOAT *f;
  SUFLOAT *gain;
  unsigned int *size;

  SU_TEST_START(ctx);

  SU_TEST_ASSERT(freq = su_test_ctx_getf_w_size(ctx, "freq", sample_count));
  SU_TEST_ASSERT(rx = su_test_ctx_getc_w_size(ctx, "rx", sample_count));

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

  rrc_block = su_block_new(
      "rrc",
      (unsigned int) (4. * 8000. / (SUFLOAT) baud),
      SU_T2N_FLOAT(8000, 1. / 468),
      0.75);
  SU_TEST_ASSERT(rrc_block != NULL);

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

  gain = su_block_get_property_ref(
      rrc_block,
      SU_BLOCK_PROPERTY_TYPE_FLOAT,
      "gain");
  SU_TEST_ASSERT(gain != NULL);

  SU_INFO(
      "Costas loop created, initial frequency: %lg Hz\n",
      SU_NORM2ABS_FREQ(*samp_rate, *f));

  SU_INFO("RRC filter gain: %lg\n", *gain);

  /* Plug wav file directly to AGC (there should be a tuner before this) */
  SU_TEST_ASSERT(su_block_plug(wav_block, 0, 0, agc_block));

  /* Plug AGC to Costas loop */
  SU_TEST_ASSERT(su_block_plug(agc_block, 0, 0, costas_block));

  /* Plug Costas loop to RRC filter */
  SU_TEST_ASSERT(su_block_plug(costas_block, 0, 0, rrc_block));

  /* Plug RRC filter to reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, rrc_block, 0));

  /* Try to read (this must work) */
  while (j < sample_count) {
    got = su_block_port_read(&port, buffer, 1);
    SU_TEST_ASSERT(got >= 0);

    if (ctx->params->dump_fmt)
      for (i = 0; i < got; ++i) {
        freq[i + j] = *f;
        rx[i + j] = buffer[i];
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

  if (rrc_block != NULL)
    su_block_destroy(rrc_block);

  if (costas_block != NULL)
    su_block_destroy(costas_block);

  if (agc_block != NULL)
    su_block_destroy(agc_block);

  if (wav_block != NULL)
    su_block_destroy(wav_block);

  return ok;
}

SUBOOL
su_test_rrc_block_with_if(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  su_block_t *rrc_block = NULL;
  su_block_t *costas_block = NULL;
  su_block_t *agc_block = NULL;
  su_block_t *tuner_block = NULL;
  su_block_t *wav_block = NULL;
  su_block_port_t port = su_block_port_INITIALIZER;
  struct su_agc_params agc_params = su_agc_params_INITIALIZER;
  SUCOMPLEX buffer[17]; /* Prime number on purpose */
  SUFLOAT samp = 0;
  SUFLOAT *freq = NULL;
  SUCOMPLEX *rx = NULL;
  int i;
  unsigned int j = 0;
  ssize_t got;

  /* Signal properties */
  const SUFLOAT baud = 468;
  const SUFLOAT arm_bw = 2 * baud;
  const unsigned int arm_order = 3;
  const SUFLOAT loop_bw = 1e-1 * baud;
  const unsigned int sample_count = 8000 * 59;
  const SUFLOAT if_off = 4000; /* IF: 1000 Hz */
  const SUFLOAT fc = 912; /* FC: 912 Hz */
  /* Block properties */
  int *samp_rate;
  SUFLOAT *f;
  SUFLOAT *gain;
  SUFLOAT *taps;
  unsigned int *size;

  SU_TEST_START(ctx);

  SU_TEST_ASSERT(freq = su_test_ctx_getf_w_size(ctx, "freq", sample_count));
  SU_TEST_ASSERT(rx = su_test_ctx_getc_w_size(ctx, "rx", sample_count));


  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 1000;
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
      SU_ABS2NORM_FREQ(*samp_rate, fc),     /* Center frequency */
      SU_ABS2NORM_FREQ(*samp_rate, baud),   /* Signal is 468 baud */
      SU_ABS2NORM_FREQ(*samp_rate, if_off), /* Move signal to 2 KHz */
      (unsigned int) (6 * SU_T2N_FLOAT(*samp_rate, 1. / baud)));
  SU_TEST_ASSERT(tuner_block != NULL);

  size = su_block_get_property_ref(
      tuner_block,
      SU_BLOCK_PROPERTY_TYPE_INTEGER,
      "size");
  SU_TEST_ASSERT(size != NULL);

  taps = su_block_get_property_ref(
      tuner_block,
      SU_BLOCK_PROPERTY_TYPE_FLOAT,
      "taps");
  SU_TEST_ASSERT(taps != NULL);

  agc_block = su_block_new("agc", &agc_params);
  SU_TEST_ASSERT(agc_block != NULL);

  costas_block = su_block_new(
      "costas",
      SU_COSTAS_KIND_QPSK,
      SU_ABS2NORM_FREQ(*samp_rate, if_off),
      SU_ABS2NORM_FREQ(*samp_rate, arm_bw),
      arm_order,
      SU_ABS2NORM_FREQ(*samp_rate, loop_bw));
  SU_TEST_ASSERT(costas_block != NULL);

  f = su_block_get_property_ref(
      costas_block,
      SU_BLOCK_PROPERTY_TYPE_FLOAT,
      "f");
  SU_TEST_ASSERT(f != NULL);

  rrc_block = su_block_new(
      "rrc",
      (unsigned int) (6 * SU_T2N_FLOAT(*samp_rate, 1. / baud)),
      SU_T2N_FLOAT(*samp_rate, 1. / baud),
      1);
  SU_TEST_ASSERT(rrc_block != NULL);

  gain = su_block_get_property_ref(
      rrc_block,
      SU_BLOCK_PROPERTY_TYPE_FLOAT,
      "gain");
  SU_TEST_ASSERT(gain != NULL);

  *gain = .707;
  SU_INFO(
      "Costas loop created, initial frequency: %lg Hz\n",
      SU_NORM2ABS_FREQ(*samp_rate, *f));

  SU_INFO("RRC filter gain: %lg\n", *gain);

  /* Plug wav file directly to tuner */
  SU_TEST_ASSERT(su_block_plug(wav_block, 0, 0, tuner_block));

  /* Plug tuner to AGC */
  SU_TEST_ASSERT(su_block_plug(tuner_block, 0, 0, agc_block));

  /* Plug AGC to Costas loop */
  SU_TEST_ASSERT(su_block_plug(agc_block, 0, 0, costas_block));

  /* Plug Costas loop to RRC filter */
  SU_TEST_ASSERT(su_block_plug(costas_block, 0, 0, rrc_block));

  /* Plug RRC filter to reading port */
  SU_TEST_ASSERT(su_block_port_plug(&port, rrc_block, 0));

  /* Try to read (this must work) */
  while (j < sample_count) {
    got = su_block_port_read(&port, buffer, 1);
    SU_TEST_ASSERT(got >= 0);

    if (ctx->params->dump_fmt)
      for (i = 0; i < got; ++i) {
        freq[i + j] = *f;
        rx[i + j] = buffer[i];
      }

    if ((j % (17 * 25)) == 0)
      SU_INFO("Center frequency: %lg Hz\r", SU_NORM2ABS_FREQ(*samp_rate, *f));
    j += got;
  }

  SU_INFO("\n");
  SU_TEST_ASSERT(SU_NORM2ABS_FREQ(*samp_rate, *f) < 1.01 * if_off &&
                 SU_NORM2ABS_FREQ(*samp_rate, *f) > 0.99 * if_off);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (su_block_port_is_plugged(&port))
    su_block_port_unplug(&port);

  if (rrc_block != NULL)
    su_block_destroy(rrc_block);

  if (costas_block != NULL)
    su_block_destroy(costas_block);

  if (agc_block != NULL)
    su_block_destroy(agc_block);

  if (tuner_block != NULL) {
    if (ctx->params->dump_fmt && taps != NULL && size != NULL) {
      ok = ok && su_test_ctx_dumpf(ctx, "bpf", taps, *size);
    }
    su_block_destroy(tuner_block);
  }

  if (wav_block != NULL)
    su_block_destroy(wav_block);

  return ok;
}


