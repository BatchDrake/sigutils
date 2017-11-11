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

#ifndef _SIGUTILS_LFSR_H
#define _SIGUTILS_LFSR_H

#include "types.h"

enum su_lfsr_mode {
  SU_LFSR_MODE_ADDITIVE,
  SU_LFSR_MODE_MULTIPLICATIVE
};

struct sigutils_lfsr {
  SUBIT *coef;   /* LFSR coefficients */
  SUBIT *buffer; /* State buffer */
  SUSCOUNT order;   /* Polynomial degree */
  enum su_lfsr_mode mode; /* LFSR mode */

  SUBIT F_prev;
  SUSCOUNT zeroes;
  SUSCOUNT p; /* Buffer pointer */
};

typedef struct sigutils_lfsr su_lfsr_t;

#define su_lfsr_INITIALIZER {NULL, NULL, 0}

SUBOOL su_lfsr_init_coef(su_lfsr_t *lfsr, const SUBIT *coef, SUSCOUNT order);
void   su_lfsr_finalize(su_lfsr_t *lfsr);
void   su_lfsr_set_mode(su_lfsr_t *lfsr, enum su_lfsr_mode mode);

void su_lfsr_set_buffer(su_lfsr_t *lfsr, const SUBIT *seq);
SUBIT su_lfsr_feed(su_lfsr_t *lfsr, SUBIT input);

/*
 * Auto-syncing mode: look for a sequence that once multiplitcatively
 * descrambled produces `order' zeroes, and switch to additive
 * scrambling right after that
 */
void su_lfsr_blind_sync_reset(su_lfsr_t *lfsr);
SUBIT su_lfsr_blind_sync_feed(su_lfsr_t *lfsr, SUBIT input);

#endif /* _SIGUTILS_LFSR_H */
