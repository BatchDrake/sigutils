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

#ifndef _SRC_TESTS_TEST_LIST_H
#define _SRC_TESTS_TEST_LIST_H

#include <test.h>

/* NCQO tests */
SUBOOL su_test_ncqo(su_test_context_t *ctx);

/* Filtering tests */
SUBOOL su_test_butterworth_lpf(su_test_context_t *ctx);

/* AGC tests */
SUBOOL su_test_agc_transient(su_test_context_t *ctx);
SUBOOL su_test_agc_steady_rising(su_test_context_t *ctx);
SUBOOL su_test_agc_steady_falling(su_test_context_t *ctx);

/* PLL tests */
SUBOOL su_test_pll(su_test_context_t *ctx);

/* Block tests */
SUBOOL su_test_block(su_test_context_t *ctx);
SUBOOL su_test_block_plugging(su_test_context_t *ctx);
SUBOOL su_test_tuner(su_test_context_t *ctx);
SUBOOL su_test_costas_block(su_test_context_t *ctx);
SUBOOL su_test_rrc_block(su_test_context_t *ctx);

/* Costas loop related tests */
SUBOOL su_test_costas_lock(su_test_context_t *ctx);
SUBOOL su_test_costas_bpsk(su_test_context_t *ctx);
SUBOOL su_test_costas_qpsk(su_test_context_t *ctx);
SUBOOL su_test_costas_qpsk_noisy(su_test_context_t *ctx);

/* Clock recovery related tests */
SUBOOL su_test_clock_recovery(su_test_context_t *ctx);
SUBOOL su_test_clock_recovery_noisy(su_test_context_t *ctx);

#endif /* _SRC_TESTS_TEST_LIST_H */
