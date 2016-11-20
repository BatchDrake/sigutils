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

#include <test_list.h>

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
      su_test_block,
      su_test_block_plugging,
      su_test_tuner,
      su_test_costas_lock,
      su_test_costas_bpsk,
      su_test_costas_qpsk
  };
  unsigned int test_count = sizeof(test_list) / sizeof(test_list[0]);

  if (!su_lib_init()) {
    SU_ERROR("Failed to initialize sigutils library\n");
    exit (EXIT_FAILURE);
  }

  su_test_run(test_list, test_count, test_count - 3, test_count - 1, SU_TRUE);

  return 0;
}

