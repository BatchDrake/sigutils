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

#ifndef _SRC_TEST_PARAM_H
#define _SRC_TEST_PARAM_H

/* AGC params */
#define SU_TEST_AGC_SIGNAL_FREQ 0.025
#define SU_TEST_AGC_WINDOW (1. / SU_TEST_AGC_SIGNAL_FREQ)

/* Costas loop related params */
#define SU_TEST_COSTAS_SYMBOL_PERIOD 0x200
#define SU_TEST_COSTAS_SIGNAL_FREQ 1e-4
#define SU_TEST_COSTAS_BANDWIDTH (1. / (2 * SU_TEST_COSTAS_SYMBOL_PERIOD))
#define SU_TEST_COSTAS_BETA .35

/* PLL params */
#define SU_TEST_PLL_SIGNAL_FREQ 0.025
#define SU_TEST_PLL_BANDWIDTH   (1e-4)

#endif /* _SRC_TEST_PARAM */
