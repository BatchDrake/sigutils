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

#include "types.h"
#include "taps.h"

SUPRIVATE void
su_taps_scale(SUFLOAT *h, SUFLOAT k, unsigned int size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *= k;
}

void
su_taps_apply_hamming(SUFLOAT *h, unsigned int size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *=
        SU_HAMMING_ALPHA - SU_MAMMING_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_apply_hann(SUFLOAT *h, unsigned int size)
{
  unsigned int i;

  for (i = 0; i < size; ++i)
    h[i] *=
        SU_HANN_ALPHA - SU_HANN_BETA * SU_COS(2 * M_PI * i / (size - 1));
}

void
su_taps_normalize_Linf(SUFLOAT *h, unsigned int size)
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
su_taps_normalize_L2(SUFLOAT *h, unsigned int size)
{
  unsigned int i;
  SUFLOAT energy = 0;

  for (i = 0; i < size; ++i)
    energy += h[i] * h[i];

  if (energy > 0)
    su_taps_scale(h, 1. / SU_SQRT(energy), size);
}

void
su_taps_normalize_L1(SUFLOAT *h, unsigned int size)
{
  unsigned int i;
  SUFLOAT amplitude = 0;

  for (i = 0; i < size; ++i)
    amplitude += SU_ABS(h[i]);

  if (amplitude > 0)
    su_taps_scale(h, 1. / amplitude, size);
}

void
su_taps_rrc_init(SUFLOAT *h, SUFLOAT T, SUFLOAT beta, unsigned int size)
{
  unsigned int i;
  SUFLOAT r_t;
  SUFLOAT dem;
  SUFLOAT num;
  SUFLOAT f;
  SUFLOAT norm = 0;

  for (i = 0; i < size; ++i) {
    r_t = (i - size / 2.) / T;
    f = 4 * beta * r_t;
    dem = M_PI * r_t * (1. - f * f);
    num = SU_SIN(M_PI * r_t * (1 - beta)) +
          4 * beta * r_t * SU_COS(M_PI * r_t * (1 + beta));

    if (SU_ABS(r_t) < SUFLOAT_THRESHOLD)
      h[i] = 1. - beta + 4. * beta / M_PI;
    else if (SU_ABS(dem) < SUFLOAT_THRESHOLD)
      h[i] = beta / SU_SQRT(2) * (
          (1 + 2 / M_PI) * sin(M_PI / (4 * beta)) +
          (1 - 2 / M_PI) * cos(M_PI / (4 * beta)));
    else
      h[i] = num / dem;

    h[i] *= 1. / T;
  }

  su_taps_apply_hamming(h, size);
}

void
su_taps_brickwall_init(SUFLOAT *h, SUFLOAT B, unsigned int size)
{
  unsigned int i;
  SUFLOAT t = 0;

  for (i = 0; i < size; ++i) {
    t = 2 * B * (i - size / 2.);
    h[i] = 2 * B * su_sinc(t);
  }

  su_taps_apply_hamming(h, size);
}

