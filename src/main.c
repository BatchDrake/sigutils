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

SUPRIVATE su_test_cb_t test_list[] = {
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
    su_test_costas_qpsk,
    su_test_costas_qpsk_noisy,
    su_test_costas_block
};

SUPRIVATE void
help(const char *argv0)
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s [options] [test_start [test_end]]\n\n", argv0);
  fprintf(
      stderr,
      "Runs all unit tests for the sigutils library. Please note that unless\n"
      "test_end is passed too, test_start will indicate an individual test\n"
      "to run. If neither test_start nor test_end is passed, all tests will\n"
      "be executed\n\n");
  fprintf(stderr, "Options:\n\n");
  fprintf(stderr, "     -d, --dump            Dump tests results to file\n");
  fprintf(stderr, "     -c, --count           Print number of tests and exit\n");
  fprintf(stderr, "     -h, --help            This help\n\n");
  fprintf(stderr, "(c) 2016 Gonzalo J. Caracedo <BatchDrake@gmail.com>\n");
}

SUPRIVATE struct option long_options[] = {
    {"dump", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"count", no_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
};

extern int optind;

int
main (int argc, char *argv[], char *envp[])
{
  unsigned int test_count = sizeof(test_list) / sizeof(test_list[0]);
  unsigned int test_start = 0;
  unsigned int test_end = test_count - 1;
  SUBOOL dump_results = SU_FALSE;
  SUBOOL result;
  int c;
  int index;

  while ((c = getopt_long(argc, argv, "dhc", long_options, &index)) != -1) {
    switch (c) {
      case 'c':
        printf("%s: %d unit tests available\n", argv[0], test_count);
        exit(EXIT_FAILURE);

      case 'd':
        dump_results = SU_TRUE;
        break;

      case 'h':
        help(argv[0]);
        exit(EXIT_SUCCESS);

      case '?':
        help(argv[0]);
        exit(EXIT_FAILURE);

      default:
        /* Should never happen */
        abort();
    }
  }

  if (!su_lib_init()) {
    fprintf(stderr, "%s: failed to initialize sigutils library\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (argc - optind >= 1) {
    if (sscanf(argv[optind++], "%u", &test_start) < 1) {
      fprintf(stderr, "%s: invalid test start `%s'\n", argv[0], argv[optind - 1]);
      exit (EXIT_FAILURE);
    }

    test_end = test_start;
  }

  if (argc - optind >= 1) {
    if (sscanf(argv[optind++], "%u", &test_end) < 1) {
      fprintf(stderr, "%s: invalid test end\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  }

  if (argc - optind >= 1) {
    fprintf(stderr, "%s: too many arguments\n", argv[0]);
    help(argv[0]);
    exit (EXIT_FAILURE);
  }

  result = su_test_run(
      test_list,
      test_count,
      SU_MIN(test_start, test_count - 1),
      SU_MIN(test_end, test_count - 1),
      dump_results);

  return !result;
}

