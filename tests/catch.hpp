/* Copyright (C) 2023 Antonio Vázquez Blanco <antoniovazquezblanco@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef _CATCH_TEST_H
#define _CATCH_TEST_H

#ifdef CATCH2_V2
    #define CATCH_CONFIG_MAIN
    #include <catch2/catch.hpp>
#endif

#ifdef CATCH2_V3
    #include <catch2/catch_all.hpp>
#endif

#if !defined(CATCH2_V2) && !defined(CATCH2_V3)
    #error "No Catch2 version specified!"
#endif

#endif