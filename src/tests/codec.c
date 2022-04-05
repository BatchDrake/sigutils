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

#include <sigutils/sigutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_list.h"
#include "test_param.h"

SUPRIVATE char su_test_symbol_to_char(SUSYMBOL sym)
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
su_test_diff_codec_generic(su_test_context_t *ctx,
                           unsigned int       bits,
                           SUBOOL             sign)
{
  su_codec_t  *encoder                            = NULL;
  su_codec_t  *decoder                            = NULL;
  SUSYMBOL     syms[SU_TEST_ENCODER_NUM_SYMS + 1] = {};
  SUSYMBOL     encoded, decoded;
  SUSCOUNT     len;
  unsigned int i;

  SUBOOL ok = SU_FALSE;

  SU_TEST_START(ctx);

  /* Initialize symbol list */
  for (i = 0; i < SU_TEST_ENCODER_NUM_SYMS; ++i)
    syms[i] = SU_TOSYM(rand() & ((1 << bits) - 1));
  syms[i] = SU_EOS;

  /* Create differential codec, 1 bit */
  SU_TEST_ASSERT(encoder = su_codec_new("diff", bits, sign));
  SU_TEST_ASSERT(decoder = su_codec_new("diff", bits, sign));

  /* Set encode mode */
  su_codec_set_direction(encoder, SU_CODEC_DIRECTION_FORWARDS);
  su_codec_set_direction(decoder, SU_CODEC_DIRECTION_BACKWARDS);

  len = sizeof(syms) / sizeof(syms[0]);
  for (i = 0; i < len; ++i) {
    encoded = su_codec_feed(encoder, syms[i]);
    decoded = su_codec_feed(decoder, encoded);

    SU_INFO("'%c' --> ENCODER --> '%c' --> DECODER --> '%c'\n",
            su_test_symbol_to_char(syms[i]),
            su_test_symbol_to_char(encoded),
            su_test_symbol_to_char(decoded));

    if (i > 0)
      SU_TEST_ASSERT(syms[i] == decoded);
  }

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (encoder != NULL)
    su_codec_destroy(encoder);
  if (decoder != NULL)
    su_codec_destroy(decoder);

  return ok;
}

SUBOOL
su_test_diff_codec_binary(su_test_context_t *ctx)
{
  return su_test_diff_codec_generic(ctx, 1, SU_FALSE);
}

SUBOOL
su_test_diff_codec_quaternary(su_test_context_t *ctx)
{
  return su_test_diff_codec_generic(ctx, 2, SU_FALSE);
}
