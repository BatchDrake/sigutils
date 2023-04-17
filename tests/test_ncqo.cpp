/* Copyright (C) 2023 Antonio Vázquez Blanco <antoniovazquezblanco@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "catch.hpp"

#include <sigutils/ncqo.h>

TEST_CASE("Test normalized frecuency scale", "[NCQO]")
{
  su_ncqo_t ncqo;
  su_ncqo_init(&ncqo, 1.0);
  REQUIRE(SUFLOAT_EQUAL(su_ncqo_read_i(&ncqo), 1.0));
  REQUIRE(SUFLOAT_EQUAL(su_ncqo_read_i(&ncqo), -1.0));
  REQUIRE(SUFLOAT_EQUAL(su_ncqo_read_i(&ncqo), 1.0));
  REQUIRE(SUFLOAT_EQUAL(su_ncqo_read_i(&ncqo), -1.0));
}
