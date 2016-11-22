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

#include <string.h>
#include "sampling.h"
#include "types.h"
#include "pll.h"
#include "taps.h"

void
su_pll_finalize(su_pll_t *pll)
{

}

SUBOOL
su_pll_init(su_pll_t *pll, SUFLOAT fhint, SUFLOAT fc)
{
  memset(pll, 0, sizeof(su_pll_t));

  pll->alpha = SU_NORM2ANG_FREQ(fc);
  pll->beta  = SU_SQRT(pll->alpha);

  su_ncqo_init(&pll->ncqo, fhint);

  return SU_TRUE;

fail:
  su_pll_finalize(pll);

  return SU_FALSE;
}

void
su_pll_feed(su_pll_t *pll, SUFLOAT x)
{
  SUCOMPLEX s;

  SUFLOAT lck = 0;
  SUFLOAT err = 0;

  s = su_ncqo_read(&pll->ncqo);

  err = -x * SU_C_IMAG(s); /* Error signal: projection against Q */
  lck =  x * SU_C_REAL(s); /* Lock: projection against I */

  pll->lock += pll->beta * (2 * lck - pll->lock);

  if (pll->ncqo.omega > -pll->alpha * err) {
    su_ncqo_inc_angfreq(&pll->ncqo, pll->alpha * err);
  }

  su_ncqo_inc_phase(&pll->ncqo, pll->beta * err);
}

/**************** QPSK Costas Filter implementation **************************/
void
su_costas_finalize(su_costas_t *costas)
{
  su_iir_filt_finalize(&costas->af);
}

SUBOOL
su_costas_init(
    su_costas_t *costas,
    enum sigutils_costas_kind kind,
    SUFLOAT fhint,
    SUFLOAT arm_bw,
    unsigned int arm_order,
    SUFLOAT loop_bw)
{
  SUFLOAT *taps = NULL;

  memset(costas, 0, sizeof(su_costas_t));

  /* Make LPF filter critically damped (Eric Hagemann) */
  costas->a = SU_NORM2ANG_FREQ(loop_bw);
  costas->b = .25 * costas->a * costas->a;
  costas->y_alpha = 1;
  costas->kind = kind;

  su_ncqo_init(&costas->ncqo, fhint);

  /* Initialize arm filters */
  if (arm_order == 0)
    arm_order = 1;
  taps = malloc(sizeof (SUFLOAT) * arm_order);
  if (taps == NULL)
    goto fail;

  if (arm_order > 1)
    su_taps_brickwall_init(taps, arm_bw, arm_order);
  else
    taps[0] = 1;

  if (!__su_iir_filt_init(&costas->af, 0, NULL, arm_order, taps, SU_FALSE))
    goto fail;

  taps = NULL;

  return SU_TRUE;

fail:
  su_costas_finalize(costas);

  if (taps != NULL)
    free(taps);

  return SU_FALSE;
}

void
su_costas_feed(su_costas_t *costas, SUCOMPLEX x)
{
  SUCOMPLEX s;
  SUCOMPLEX L;
  SUFLOAT e = 0;

  s = su_ncqo_read(&costas->ncqo);
  /*
   * s = cos(wt) + sin(wt). Signal sQ be 90 deg delayed wrt sI, therefore
   * we must multiply by conj(s).
   */
  costas->z = su_iir_filt_feed(&costas->af, conj(s) * x);

  switch (costas->kind) {
    case SU_COSTAS_KIND_NONE:
      SU_ERROR("Invalid Costas loop\n");
      return;

    case SU_COSTAS_KIND_BPSK:
      /* Taken directly from Wikipedia */
      e = -SU_C_REAL(costas->z) * SU_C_IMAG(costas->z);
      break;

    case SU_COSTAS_KIND_QPSK:
      /* Compute limiter output */
      L = SU_C_SGN(costas->z);

      /*
       * Error signal taken from Maarten Tytgat's paper "Time Domain Model
       * for Costas Loop Based QPSK Receiver.
       */
      e =  SU_C_REAL(L) * SU_C_IMAG(costas->z)
          -SU_C_IMAG(L) * SU_C_REAL(costas->z);
      break;

    default:
          SU_ERROR("Unsupported Costas loop kind\n");
          return;
  }

  costas->lock += costas->a * (1 - e - costas->lock);
  costas->y += costas->y_alpha * (costas->z - costas->y);

  /* IIR loop filter suggested by Eric Hagemann */
  su_ncqo_inc_angfreq(&costas->ncqo, costas->b * e);
  su_ncqo_inc_phase(&costas->ncqo, costas->a * e);

}

