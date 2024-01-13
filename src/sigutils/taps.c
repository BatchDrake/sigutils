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

#include <sigutils/taps.h>

#include <sigutils/sampling.h>
#include <sigutils/types.h>

/************************* Real window functions *****************************/
SUPRIVATE void
su_taps_scale(SUFLOAT *h, SUFLOAT k, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= k;
}

void
su_taps_apply_hamming(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *=
        SU_HAMMING_ALPHA - SU_MAMMING_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_apply_hann(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_HANN_ALPHA - SU_HANN_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_apply_flat_top(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_FLAT_TOP_A0 - SU_FLAT_TOP_A1 * SU_COS(2 * M_PI * i / (size - 1))
            + SU_FLAT_TOP_A2 * SU_COS(4 * M_PI * i / (size - 1))
            - SU_FLAT_TOP_A3 * SU_COS(6 * M_PI * i / (size - 1))
            + SU_FLAT_TOP_A1 * SU_COS(8 * M_PI * i / (size - 1));
}

void
su_taps_apply_blackmann_harris(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_BLACKMANN_HARRIS_A0
            - SU_BLACKMANN_HARRIS_A1 * SU_COS(2 * M_PI * i / (size - 1))
            + SU_BLACKMANN_HARRIS_A2 * SU_COS(4 * M_PI * i / (size - 1))
            - SU_BLACKMANN_HARRIS_A3 * SU_COS(6 * M_PI * i / (size - 1));
}

/********************** Complex window functions ****************************/
void
su_taps_apply_hamming_complex(SUCOMPLEX *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *=
        SU_HAMMING_ALPHA - SU_MAMMING_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_apply_hann_complex(SUCOMPLEX *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_HANN_ALPHA - SU_HANN_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_apply_flat_top_complex(SUCOMPLEX *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_FLAT_TOP_A0 - SU_FLAT_TOP_A1 * SU_COS(2 * M_PI * i / (size - 1))
            + SU_FLAT_TOP_A2 * SU_COS(4 * M_PI * i / (size - 1))
            - SU_FLAT_TOP_A3 * SU_COS(6 * M_PI * i / (size - 1))
            + SU_FLAT_TOP_A1 * SU_COS(8 * M_PI * i / (size - 1));
}

void
su_taps_apply_blackmann_harris_complex(SUCOMPLEX *h, SUSCOUNT size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= SU_BLACKMANN_HARRIS_A0
            - SU_BLACKMANN_HARRIS_A1 * SU_COS(2 * M_PI * i / (size - 1))
            + SU_BLACKMANN_HARRIS_A2 * SU_COS(4 * M_PI * i / (size - 1))
            - SU_BLACKMANN_HARRIS_A3 * SU_COS(6 * M_PI * i / (size - 1));
}

void
su_taps_normalize_Linf(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT max = 0;

  for (i = 0; i < size; ++i)
    if (SU_ABS(h[i]) > max)
      max = SU_ABS(h[i]);

  if (max > 0)
    su_taps_scale(h, 1. / max, size);
}

void
su_taps_normalize_L2(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT energy = 0;

  for (i = 0; i < size; ++i)
    energy += h[i] * h[i];

  if (energy > 0)
    su_taps_scale(h, 1. / SU_SQRT(energy), size);
}

void
su_taps_normalize_L1(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT amplitude = 0;

  for (i = 0; i < size; ++i)
    amplitude += SU_ABS(h[i]);

  if (amplitude > 0)
    su_taps_scale(h, 1. / amplitude, size);
}

/***************************** Hilbert transform *****************************/
void
su_taps_hilbert_init(SUFLOAT *h, SUSCOUNT size)
{
  unsigned int i;
  int n = -size / 2;

  for (i = 0; i < size; ++i, ++n)
    h[i] = 2. / (M_PI * (n - .5f));

  su_taps_apply_hamming(h, size);
}

void
su_taps_rrc_init(SUFLOAT *h, SUFLOAT T, SUFLOAT beta, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT r_t;
  SUFLOAT dem;
  SUFLOAT num;
  SUFLOAT f;

  for (i = 0; i < size; ++i) {
    r_t = (i - size / 2.) / T;
    f = 4 * beta * r_t;
    dem = M_PI * r_t * (1. - f * f);
    num = SU_SIN(M_PI * r_t * (1 - beta))
          + 4 * beta * r_t * SU_COS(M_PI * r_t * (1 + beta));

    if (SU_ABS(r_t) < SUFLOAT_THRESHOLD)
      h[i] = 1. - beta + 4. * beta / M_PI;
    else if (SU_ABS(dem) < SUFLOAT_THRESHOLD)
      h[i] = beta / SU_SQRT(2)
             * ((1 + 2 / M_PI) * sin(M_PI / (4 * beta))
                + (1 - 2 / M_PI) * cos(M_PI / (4 * beta)));
    else
      h[i] = num / dem;

    h[i] *= 1. / T;
  }

  su_taps_apply_hamming(h, size);
}

void
su_taps_brickwall_lp_init(SUFLOAT *h, SUFLOAT fc, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT t = 0;

  for (i = 0; i < size; ++i) {
    t = i - (size >> 1);
    h[i] = fc * su_sinc(fc * t);
  }

  su_taps_apply_hamming(h, size);
}

void
su_taps_brickwall_bp_init(SUFLOAT *h, SUFLOAT bw, SUFLOAT if_nor, SUSCOUNT size)
{
  unsigned int i;
  SUFLOAT t = 0;
  SUFLOAT omega = SU_NORM2ANG_FREQ(if_nor);

  /*
   * If intermediate frequency is lesser than or equal to half the bandwidth,
   * the product-by-cosine trick will not work: negative and positive
   * pass bands will overlap, creating a spurious gain of 6 dB in the
   * intersection. There are two ways to overcome this:
   *
   * a) Replace SU_COS() by SU_C_EXP and use complex coefficients. This
   *    implies changing the IIR filter implementation. Costly.
   * b) Detect this situation, and if if_nor <= bw / 2, use a
   *    low pass filter with fc = if_nor + bw / 2
   */

  if (if_nor <= .5 * bw) {
    su_taps_brickwall_lp_init(h, if_nor + .5 * bw, size);
  } else {
    for (i = 0; i < size; ++i) {
      t = i - .5 * size;
      h[i] = bw * su_sinc(.5 * bw * t) * SU_COS(omega * t);
    }

    su_taps_apply_hamming(h, size);
  }
}
