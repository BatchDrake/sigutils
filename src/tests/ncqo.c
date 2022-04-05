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

#include <sigutils/agc.h>
#include <sigutils/iir.h>
#include <sigutils/ncqo.h>
#include <sigutils/pll.h>
#include <sigutils/sampling.h>
#include <sigutils/sigutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_list.h"
#include "test_param.h"

SUBOOL
su_test_ncqo(su_test_context_t *ctx)
{
  SUBOOL       ok     = SU_FALSE;
  SUFLOAT     *buffer = NULL;
  unsigned int p      = 0;
  su_ncqo_t    ncqo   = su_ncqo_INITIALIZER;

  SU_TEST_ASSERT(buffer = su_test_buffer_new(SU_TEST_SIGNAL_BUFFER_SIZE));

  SU_TEST_START(ctx);

  su_ncqo_init(&ncqo, 1);

  /* Test in-phase signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_i(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE),
                    0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 2));

  /* Test quadrature signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_q(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE),
                    0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  /* Modify phase */
  su_ncqo_set_phase(&ncqo, PI / 2);

  /* Test in-phase signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_i(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE),
                    0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 0));

  /* Test quadrature signal */
  for (p = 0; p < SU_TEST_SIGNAL_BUFFER_SIZE; ++p)
    buffer[p] = su_ncqo_read_q(&ncqo);

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_mean(buffer, SU_TEST_SIGNAL_BUFFER_SIZE),
                    0));

  SU_TEST_ASSERT(
      SUFLOAT_EQUAL(su_test_buffer_pp(buffer, SU_TEST_SIGNAL_BUFFER_SIZE), 2));

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
