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

#include <sigutils/pll.h>

#include <string.h>

#include <sigutils/coef.h>
#include <sigutils/log.h>
#include <sigutils/sampling.h>
#include <sigutils/taps.h>
#include <sigutils/types.h>

SU_DESTRUCTOR(su_pll)
{
}

SU_CONSTRUCTOR(su_pll, SUFLOAT fhint, SUFLOAT fc)
{
  memset(self, 0, sizeof(su_pll_t));

  su_pll_set_cutoff(self, fc);
  su_ncqo_init(&self->ncqo, fhint);

  return SU_TRUE;
}

SU_METHOD(su_pll, SUCOMPLEX, track, SUCOMPLEX x)
{
  SUCOMPLEX ref = su_ncqo_read(&self->ncqo);
  SUCOMPLEX mix = x * SU_C_CONJ(ref);
  SUFLOAT phase = su_ncqo_get_phase(&self->ncqo);
  SUFLOAT error = su_phase_adjust_one_cycle(SU_C_ARG(x) - phase);

  su_ncqo_inc_angfreq(&self->ncqo, self->alpha * error);
  su_ncqo_inc_phase(&self->ncqo, self->beta * error);
  SU_SPLPF_FEED(self->lock, SU_C_REAL(mix), self->alpha);

  return mix;
}

SU_METHOD(su_pll, void, feed, SUFLOAT x)
{
  SUCOMPLEX s;

  SUFLOAT lck = 0;
  SUFLOAT err = 0;

  s = su_ncqo_read(&self->ncqo);

  err = -x * SU_C_IMAG(s); /* Error signal: projection against Q */
  lck = x * SU_C_REAL(s);  /* Lock: projection against I */

  self->lock += self->beta * (2 * lck - self->lock);

  if (self->ncqo.omega > -self->alpha * err) {
    su_ncqo_inc_angfreq(&self->ncqo, self->alpha * err);
  }

  su_ncqo_inc_phase(&self->ncqo, self->beta * err);
}

/**************** QPSK Costas Filter implementation **************************/
SU_DESTRUCTOR(su_costas)
{
  SU_DESTRUCT(su_iir_filt, &self->af);
}

SU_CONSTRUCTOR(
    su_costas,
    enum sigutils_costas_kind kind,
    SUFLOAT fhint,
    SUFLOAT arm_bw,
    unsigned int arm_order,
    SUFLOAT loop_bw)
{
  SUFLOAT *b = NULL;
  SUFLOAT *a = NULL;
  SUFLOAT scaling;
  unsigned int i = 0;

  memset(self, 0, sizeof(su_costas_t));

  /* Make LPF filter critically damped (Eric Hagemann) */
  self->a = SU_NORM2ANG_FREQ(loop_bw);
  self->b = .5 * self->a * self->a;
  self->y_alpha = 1;
  self->kind = kind;
  self->gain = 1;

  su_ncqo_init(&self->ncqo, fhint);

  /* Initialize arm filters */
  if (arm_order == 0)
    arm_order = 1;

  if (arm_order == 1 || arm_order >= SU_COSTAS_FIR_ORDER_THRESHOLD) {
    SU_ALLOCATE_MANY_FAIL(b, arm_order, SUFLOAT);

    if (arm_order == 1)
      b[0] = 1; /* No filtering */
    else
      su_taps_brickwall_lp_init(b, arm_bw, arm_order);
  } else {
    /* If arm filter order is small, try to build a IIR filter */
    SU_TRY_FAIL(a = su_dcof_bwlp(arm_order - 1, arm_bw));
    SU_TRY_FAIL(b = su_ccof_bwlp(arm_order - 1));

    scaling = su_sf_bwlp(arm_order - 1, arm_bw);

    for (i = 0; i < arm_order; ++i)
      b[i] *= scaling;
  }

  SU_TRY_FAIL(__su_iir_filt_init(
      &self->af,
      a == NULL ? 0 : arm_order,
      a,
      arm_order,
      b,
      SU_FALSE));

  b = NULL;
  a = NULL;

  return SU_TRUE;

fail:
  SU_DESTRUCT(su_costas, self);

  if (b != NULL)
    free(b);

  if (a != NULL)
    free(a);

  return SU_FALSE;
}

SU_METHOD(su_costas, void, set_loop_gain, SUFLOAT gain)
{
  self->gain = gain;
}

SU_METHOD(su_costas, SUCOMPLEX, feed, SUCOMPLEX x)
{
  SUCOMPLEX s;
  SUCOMPLEX L;
  SUFLOAT e = 0;

  s = su_ncqo_read(&self->ncqo);
  /*
   * s = cos(wt) + sin(wt). Signal sQ be 90 deg delayed wrt sI, therefore
   * we must multiply by conj(s).
   */
  self->z = self->gain * su_iir_filt_feed(&self->af, SU_C_CONJ(s) * x);

  switch (self->kind) {
    case SU_COSTAS_KIND_NONE:
      SU_ERROR("Invalid Costas loop\n");
      return 0;

    case SU_COSTAS_KIND_BPSK:
      /* Taken directly from Wikipedia */
      e = -SU_C_REAL(self->z) * SU_C_IMAG(self->z);
      break;

    case SU_COSTAS_KIND_QPSK:
      /* Compute limiter output */
      L = SU_C_SGN(self->z);

      /*
       * Error signal taken from Maarten Tytgat's paper "Time Domain Model
       * for Costas Loop Based QPSK Receiver.
       */
      e = SU_C_REAL(L) * SU_C_IMAG(self->z) - SU_C_IMAG(L) * SU_C_REAL(self->z);
      break;

    case SU_COSTAS_KIND_8PSK:
      /*
       * The following phase detector was shamelessly borrowed from
       * GNU Radio's Costas Loop implementation. I'm keeping the
       * original comment for referece:
       *
       * -----------8<--------------------------------------------------
       * This technique splits the 8PSK constellation into 2 squashed
       * QPSK constellations, one when I is larger than Q and one
       * where Q is larger than I. The error is then calculated
       * proportionally to these squashed constellations by the const
       * K = sqrt(2)-1.
       *
       * The signal magnitude must be > 1 or K will incorrectly bias
       * the error value.
       *
       * Ref: Z. Huang, Z. Yi, M. Zhang, K. Wang, "8PSK demodulation for
       * new generation DVB-S2", IEEE Proc. Int. Conf. Communications,
       * Circuits and Systems, Vol. 2, pp. 1447 - 1450, 2004.
       * -----------8<--------------------------------------------------
       */

      L = SU_C_SGN(self->z);

      if (SU_ABS(SU_C_REAL(self->z)) >= SU_ABS(SU_C_IMAG(self->z)))
        e = SU_C_REAL(L) * SU_C_IMAG(self->z)
            - SU_C_IMAG(L) * SU_C_REAL(self->z) * (SU_SQRT2 - 1);
      else
        e = SU_C_REAL(L) * SU_C_IMAG(self->z) * (SU_SQRT2 - 1)
            - SU_C_IMAG(L) * SU_C_REAL(self->z);
      break;

    default:
      SU_ERROR("Unsupported Costas loop kind\n");
      return 0;
  }

  self->lock += self->a * (1 - e - self->lock);
  self->y += self->y_alpha * (self->z - self->y);

  /* IIR loop filter suggested by Eric Hagemann */
  su_ncqo_inc_angfreq(&self->ncqo, self->b * e);
  su_ncqo_inc_phase(&self->ncqo, self->a * e);

  return self->y;
}

SU_METHOD(su_costas, void, set_kind, enum sigutils_costas_kind kind)
{
  self->kind = kind;
}
