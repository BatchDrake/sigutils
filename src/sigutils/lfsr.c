/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

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

#define SU_LOG_LEVEL "lfsr"

#include <sigutils/lfsr.h>

#include <string.h>

#include <sigutils/log.h>

SUBOOL
su_lfsr_init_coef(su_lfsr_t *lfsr, const SUBITS *coef, SUSCOUNT order)
{
  memset(lfsr, 0, sizeof(su_lfsr_t));

  SU_TRYCATCH(lfsr->coef = malloc(order * sizeof(SUBITS)), goto fail);

  SU_TRYCATCH(lfsr->buffer = calloc(order, sizeof(SUBITS)), goto fail);

  memcpy(lfsr->coef, coef, order * sizeof(SUBITS));
  lfsr->order = order;

  return SU_TRUE;

fail:
  su_lfsr_finalize(lfsr);

  return SU_FALSE;
}

void
su_lfsr_finalize(su_lfsr_t *lfsr)
{
  if (lfsr->coef != NULL)
    free(lfsr->coef);

  if (lfsr->buffer != NULL)
    free(lfsr->buffer);
}

void
su_lfsr_set_mode(su_lfsr_t *lfsr, enum su_lfsr_mode mode)
{
  lfsr->mode = mode;
}

/*
 * There is a common part for both additive and multiplicative
 * descramblers, related to the shift register.
 *
 * Assuming that we have some function with memory F(P, x) which
 * returns the evaluation of some Z2 polynomial P taking x
 * as the new element in the shift reg:
 *
 * ADDITIVE DESCRAMBLER:
 *   y = F(P, F_prev) ^ x
 *
 * MULTIPLICATIVE DESCRAMBLER:
 *   y = F(P, x) ^ x
 *
 */
SUINLINE SUBITS
su_lfsr_transfer(su_lfsr_t *lfsr, SUBITS x)
{
  SUBITS F = 0;
  uint64_t i;
  uint64_t n = lfsr->p;

  for (i = 1; i < lfsr->order; ++i) {
    if (++n == lfsr->order)
      n = 0;

    if (lfsr->coef[i])
      F ^= lfsr->buffer[n];
  }

  /*
   * When we leave the loop (i.e. when i = order),
   * n = (p + order - 1) % order = (p - 1) % order
   */
  lfsr->buffer[lfsr->p] = x;
  lfsr->p = n;

  return F;
}

void
su_lfsr_set_buffer(su_lfsr_t *lfsr, const SUBITS *seq)
{
  unsigned int i;

  for (i = 0; i < lfsr->order; ++i)
    lfsr->buffer[lfsr->order - i - 1] = seq[i];
  lfsr->p = lfsr->order - 1;
}

SUBITS
su_lfsr_feed(su_lfsr_t *lfsr, SUBITS x)
{
  SUBITS y;
  x = !!x;

  switch (lfsr->mode) {
    case SU_LFSR_MODE_ADDITIVE:
      lfsr->F_prev = su_lfsr_transfer(lfsr, lfsr->F_prev);
      y = lfsr->F_prev ^ x;
      break;

    case SU_LFSR_MODE_MULTIPLICATIVE:
      y = su_lfsr_transfer(lfsr, x) ^ x;
      break;

    default:
      y = SU_NOSYMBOL;
  }

  return y;
}

void
su_lfsr_blind_sync_reset(su_lfsr_t *lfsr)
{
  lfsr->zeroes = 0;
  su_lfsr_set_mode(lfsr, SU_LFSR_MODE_MULTIPLICATIVE);
  memset(lfsr->buffer, 0, lfsr->order * sizeof(SUBITS));
}

SUBITS
su_lfsr_blind_sync_feed(su_lfsr_t *lfsr, SUBITS x)
{
  SUBITS y = x;

  y = su_lfsr_feed(lfsr, x);

  /* LFSR running in scan mode */
  if (lfsr->mode == SU_LFSR_MODE_MULTIPLICATIVE) {
    if (y != 0)
      lfsr->zeroes = 0;
    else if (++lfsr->zeroes == 2 * lfsr->order) {
      /* Synchronization sequence found! Switch to additive */
      su_lfsr_set_mode(lfsr, SU_LFSR_MODE_ADDITIVE);
      printf("Sync sequence found!\n");
      lfsr->zeroes = 0;
    }
  }

  return y;
}
