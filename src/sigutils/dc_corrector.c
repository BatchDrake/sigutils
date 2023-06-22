/*

  Copyright (C) 2023 Gonzalo José Carracedo Carballal

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

#include <sigutils/dc_corrector.h>
#include <string.h>


SU_METHOD(su_dc_corrector, SUBOOL, init_with_training_period, SUSCOUNT train)
{
  memset(self, 0, sizeof (su_dc_corrector_t));

  self->soft_dc_train_samples = train;
  self->training = SU_TRUE;

  return SU_TRUE;
}

SU_METHOD(su_dc_corrector, SUBOOL, init_with_alpha, SUFLOAT alpha)
{
  memset(self, 0, sizeof (su_dc_corrector_t));

  self->soft_dc_alpha = alpha;
  self->training = SU_TRUE;
  self->have_dc_offset = SU_TRUE;

  return SU_TRUE;
}

SU_METHOD(su_dc_corrector, void, set_training_state, SUBOOL enabled)
{
  self->training = enabled;
}

SU_METHOD(su_dc_corrector, void, reset)
{
  self->soft_dc_count = 0;
  self->dc_offset     = 0;
  self->dc_c          = 0;

  if (self->soft_dc_train_samples > 0)
    self->have_dc_offset = SU_FALSE;
}

SU_METHOD(su_dc_corrector, void, correct, SUCOMPLEX *buffer, SUSCOUNT result)
{
  SUCOMPLEX y, c, t, sum;
  SUSCOUNT i;

  if (self->training) {
    c   = self->dc_c;
    sum = self->dc_offset;

    for (i = 0; i < result; ++i) {
      y = buffer[i] - c;
      t = sum + y;
      c = (t - sum) - y;
      sum = t;
    }

    if (self->soft_dc_train_samples > 0) {
      /* Correction based on training samples */
      self->dc_c = c;
      self->dc_offset = sum;
      self->soft_dc_count += result;

      if (self->soft_dc_count > self->soft_dc_train_samples) {
        self->dc_offset /= self->soft_dc_count;
        self->training = SU_FALSE;
        self->have_dc_offset = SU_TRUE;
      }
    } else {
      /* Correction based on single-pole filters */
      SU_SPLPF_FEED(self->dc_offset, sum / result, self->soft_dc_alpha);
    }
  }
    
  if (self->have_dc_offset)
    for (i = 0; i < result; ++i)
      buffer[i] -= self->dc_offset;
}
