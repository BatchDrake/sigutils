/*
 * main.c: entry point for sigutils
 * Creation date: Thu Oct 20 22:56:46 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <sigutils/sampling.h>
#include <sigutils/ncqo.h>
#include <sigutils/iir.h>
#include <sigutils/agc.h>
#include <sigutils/pll.h>

#include <sigutils/sigutils.h>

#include "test.h"

#define SU_TEST_AGC_SIGNAL_FREQ 0.025
#define SU_TEST_AGC_WINDOW (1. / SU_TEST_AGC_SIGNAL_FREQ)

#define SU_TEST_PLL_SIGNAL_FREQ 0.025
#define SU_TEST_PLL_BANDWIDTH   (1e-4)

SUBOOL
su_test_ncqo(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *buffer = NULL;
  unsigned int p = 0;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;

  SU_TEST_ASSERT(buffer = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));

  SU_TEST_START(ctx);

  su_ncqo_init(&ncqo, 1);

  /* Test in-phase signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_i(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 2));

  /* Test quadrature signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_q(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  /* Modify phase */
  su_ncqo_set_phase(&ncqo, PI / 2);

  /* Test in-phase signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_i(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  /* Test quadrature signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_q(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(
          su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 2));

  /* Test constant signal */
  su_ncqo_set_phase(&ncqo, 0);
  su_ncqo_set_freq(&ncqo, 0);

  /* Test in-phase signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    SU_TEST_ASSERT(SUFLOAT_EQUAL(su_ncqo_read_i(&ncqo), 1));
    SU_TEST_ASSERT(SUFLOAT_EQUAL(su_ncqo_read_q(&ncqo), 0));
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (buffer != NULL)
    free(buffer);

  return ok;
}

SUBOOL
su_test_butterworth_lpf(su_test_context_t *ctx)
{
  SUBOOL ok = SU_FALSE;
  SUFLOAT *buffer = NULL;
  unsigned int p = 0;
  su_ncqo_t ncqo = su_ncqo_INITIALIZER;
  su_iir_filt_t lpf = su_iir_filt_INITIALIZER;

  SU_TEST_START_TICKLESS(ctx);

  SU_TEST_ASSERT(buffer = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));

  SU_TEST_ASSERT(su_iir_bwlpf_init(&lpf, 5, 0.25));

  SU_TEST_TICK(ctx);

  su_ncqo_init(&ncqo, 1);

  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_iir_filt_feed(
        &lpf,
        su_ncqo_read_i(&ncqo));

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            buffer,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "hi.m",
            "hi"));

    printf(" hi pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" hi mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" hi std: " SUFLOAT_FMT "\n", su_test_buffer_std(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  }

  su_ncqo_set_freq(&ncqo, .125);

  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_iir_filt_feed(
        &lpf,
        su_ncqo_read_i(&ncqo));

  if (ctx->dump_results) {
    SU_TEST_ASSERT(
        su_test_buffer_dump_matlab(
            buffer,
            SU_TEST_SIGNAL_BUFFER_SIZE,
            "lo.m",
            "lo"));

    printf(" lo pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" lo mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
    printf(" lo std: " SUFLOAT_FMT "\n", su_test_buffer_std(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (buffer != NULL)
    free(buffer);

  su_iir_filt_finalize(&lpf);

  return ok;
}

SUBOOL
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

  if (ctx->dump_results) {
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
    buffer[p] = su_agc_feed(&agc, buffer[p]);
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  su_agc_finalize(&agc);

  if (buffer != NULL) {
    if (ctx->dump_results) {
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

  if (ctx->dump_results) {
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
    buffer[p] = su_agc_feed(&agc, buffer[p]);
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
    if (ctx->dump_results) {
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

  /* Create a falling sinusoid */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p) {
    t = p - SU_TEST_SIGNAL_BUFFER_SIZE / 2;
    buffer[p] = 1e-2 * rand() / (double) RAND_MAX;

    buffer[p] += 0.2 * floor(5 - 5 * p / SU_TEST_SIGNAL_BUFFER_SIZE) * su_ncqo_read_i(&ncqo);
  }

  if (ctx->dump_results) {
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
    buffer[p] = su_agc_feed(&agc, buffer[p]);
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
    if (ctx->dump_results) {
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

int
main (int argc, char *argv[], char *envp[])
{
  su_test_cb_t test_list[] = {
      su_test_ncqo,
      su_test_butterworth_lpf,
      su_test_agc_transient,
      su_test_agc_steady_rising,
      su_test_agc_steady_falling,
      su_test_pll,
      su_test_block
  };
  unsigned int test_count = sizeof(test_list) / sizeof(test_list[0]);

  if (!su_lib_init()) {
    SU_ERROR("Failed to initialize sigutils library\n");
    exit (EXIT_FAILURE);
  }

  su_test_run(test_list, test_count, test_count - 1, test_count - 1, SU_TRUE);

  return 0;
}

