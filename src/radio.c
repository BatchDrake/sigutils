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

int
main(int argc, char *argv[])
{
  su_modem_t *modem = NULL;
  SUCOMPLEX sample = 0;
  unsigned int count = 0;

  if (argc != 2) {
    fprintf(stderr, "Usage:\n\t%s file.wav\n");
    exit(EXIT_FAILURE);
  }

  if (!su_lib_init()) {
    fprintf(stderr, "%s: failed to initialize library\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((modem = su_modem_new("qpsk")) == NULL) {
    fprintf(stderr, "%s: failed to initialize QPSK modem\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!su_modem_set_wav_source(modem, argv[1])) {
    fprintf(
        stderr,
        "%s: failed to set modem wav source to %s\n",
        argv[0],
        argv[1]);
    exit(EXIT_FAILURE);
  }

  su_modem_set_bool(modem, "abc", SU_FALSE);
  su_modem_set_bool(modem, "afc", SU_TRUE);

  su_modem_set_int(modem, "mf_span", 4);
  su_modem_set_float(modem, "baud", 468);
  su_modem_set_float(modem, "fc", 910);
  su_modem_set_float(modem, "rolloff", .25);

  if (!su_modem_start(modem)) {
    fprintf(stderr, "%s: failed to start modem\n", argv[0]);
    exit(EXIT_FAILURE);
  }


  puts("rx = [");

  while (!isnan(SU_C_ABS(sample = su_modem_read_sample(modem)))) {
    printf(
        "%s  %3.12lf + %3.12lfi",
        count == 0 ? "" : ";\n",
        SU_C_REAL(sample),
        SU_C_IMAG(sample));
    ++count;
  }

  puts("\n];");

  SU_INFO("%d samples recovered\n", count);

  su_modem_destroy(modem);

  return 0;
}
