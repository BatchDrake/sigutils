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

#include <sigutils/sigutils.h>

#include "test_list.h"
#include "test_param.h"

#define NUM_SYMS 32

SUPRIVATE char
su_test_symbol_to_char(SUSYMBOL sym)
{
  switch (sym) {
    case SU_NOSYMBOL:
      return ' ';

    case SU_EOS:
      return '%';

    default:
      return sym;
  }
}

SUBOOL
su_test_diff_encoder_bpsk(su_test_context_t *ctx)
{
  su_encoder_t *encoder = NULL;
  su_encoder_t *decoder = NULL;
  SUSYMBOL syms[NUM_SYMS + 1] = {};
  SUSYMBOL encoded, decoded;
  SUSCOUNT len;
  unsigned int i;

  SUBOOL ok = SU_FALSE;

  SU_TEST_START(ctx);

  /* Initialize symbol list */
  for (i = 0; i < NUM_SYMS; ++i)
    syms[i] = SU_TOSYM(rand() & 1);
  syms[i] = SU_EOS;

  /* Create differential encoder, 1 bit */
  SU_TEST_ASSERT(encoder = su_encoder_new("diff", 1, SU_FALSE));
  SU_TEST_ASSERT(decoder = su_encoder_new("diff", 1, SU_FALSE));

  /* Set encode mode */
  su_encoder_set_direction(encoder, SU_ENCODER_DIRECTION_FORWARDS);
  su_encoder_set_direction(decoder, SU_ENCODER_DIRECTION_BACKWARDS);

  len = sizeof(syms) / sizeof(syms[0]);
  for (i = 0; i < len; ++i) {
    encoded = su_encoder_feed(encoder, syms[i]);
    decoded = su_encoder_feed(decoder, encoded);

    if (i > 0)
      SU_TEST_ASSERT(syms[i] == decoded);

    SU_INFO(
        "'%c' --> ENCODER --> '%c' --> DECODER --> '%c'\n",
        su_test_symbol_to_char(syms[i]),
        su_test_symbol_to_char(encoded),
        su_test_symbol_to_char(decoded));
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (encoder != NULL)
    su_encoder_destroy(encoder);
  if (decoder != NULL)
    su_encoder_destroy(decoder);

  return ok;
}
