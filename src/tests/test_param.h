/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#ifndef _SRC_TEST_PARAM_H
#define _SRC_TEST_PARAM_H

/* Block-related params */
#define SU_TEST_BLOCK_SAWTOOTH_WIDTH 119
#define SU_TEST_BLOCK_READ_WAIT_MS 25

/* Preferred matched filter span (in symbol periods) */
#define SU_TEST_MF_SYMBOL_SPAN 6

/* Default test buffer size */
#define SU_TEST_SIGNAL_BUFFER_SIZE (32 * 4096)

/* AGC params */
#define SU_TEST_AGC_SIGNAL_FREQ 0.025
#define SU_TEST_AGC_WINDOW (1. / SU_TEST_AGC_SIGNAL_FREQ)

/* Costas loop related params */
#define SU_TEST_COSTAS_SYMBOL_PERIOD 0x200
#define SU_TEST_COSTAS_SIGNAL_FREQ 1e-2
#define SU_TEST_COSTAS_BANDWIDTH (.5 / (SU_TEST_COSTAS_SYMBOL_PERIOD))
#define SU_TEST_COSTAS_BETA .35

/* PLL params */
#define SU_TEST_PLL_SIGNAL_FREQ 0.025
#define SU_TEST_PLL_BANDWIDTH (1e-4)

/* Channel detector params */
#define SU_TEST_USE_ACTUAL_CAPTURE

#ifdef SU_TEST_USE_ACTUAL_CAPTURE
#define SU_CHANNEL_DETECTOR_SAMPLE_CAPTURE \
  "gqrx_20170219_185002_1545346100_250000_fc.raw"
#else
#define SU_CHANNEL_DETECTOR_SAMPLE_CAPTURE \
  "su_test_channel_detector_qpsk/tx-complex.raw"
#endif

/* Encoder parameters */
#define SU_TEST_ENCODER_NUM_SYMS 32

/* Spectral tuner */
#define SU_TEST_SPECTTUNER_FREQ1 200.
#define SU_TEST_SPECTTUNER_FREQ2 1000.

#define SU_TEST_SPECTTUNER_SAMP_RATE 8000.
#define SU_TEST_SPECTTUNER_N0 2e-2

#endif /* _SRC_TEST_PARAM */
