/*
 * main.c: entry point for sigutils
 * Creation date: Thu Oct 20 22:56:46 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <sigutils.h>

#include <sigutils/sampling.h>
#include <sigutils/ncqo.h>
#include <sigutils/iir.h>
#include "test.h"

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

#ifdef SU_DUMP_SIGNALS
  SU_TEST_ASSERT(
      su_test_buffer_dump_matlab(
          buffer,
          SU_TEST_SIGNAL_BUFFER_SIZE,
          "hi.m",
          "hi"));

  printf(" hi pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  printf(" hi mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  printf(" hi std: " SUFLOAT_FMT "\n", su_test_buffer_std(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
#endif

  su_ncqo_set_freq(&ncqo, .125);

  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_iir_filt_feed(
        &lpf,
        su_ncqo_read_i(&ncqo));

#ifdef SU_DUMP_SIGNALS
  SU_TEST_ASSERT(
      su_test_buffer_dump_matlab(
          buffer,
          SU_TEST_SIGNAL_BUFFER_SIZE,
          "lo.m",
          "lo"));

  printf(" lo pp: " SUFLOAT_FMT "\n", su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  printf(" lo mean: " SUFLOAT_FMT "\n", su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
  printf(" lo std: " SUFLOAT_FMT "\n", su_test_buffer_std(buffer, SU_TEST_SIGNAL_BUFFER_SIZE));
#endif

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (buffer != NULL)
    free(buffer);

  su_iir_filt_finalize(&lpf);

  return ok;
}


int
main (int argc, char *argv[], char *envp[])
{
  su_test_cb_t test_list[] = {
      su_test_ncqo,
      su_test_butterworth_lpf};
  unsigned int test_count = sizeof(test_list) / sizeof(test_list[0]);


  su_test_run(test_list, test_count, 0, test_count - 1);

  return 0;
}

