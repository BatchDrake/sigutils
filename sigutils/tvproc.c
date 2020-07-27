/*

  Copyright (C) 2020 Gonzalo José Carracedo Carballal

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

#define SU_LOG_DOMAIN "tvproc"

#include "log.h"
#include "tvproc.h"
#include "sampling.h"

/***************************** Pulse finder ***********************************/
su_pulse_finder_t *
su_pulse_finder_new(
    SUFLOAT base,
    SUFLOAT peak,
    SUSCOUNT len,
    SUFLOAT tolerance)
{
  su_pulse_finder_t *new = NULL;
  SUFLOAT *coef = NULL;
  unsigned int i;
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(new  = calloc(1, sizeof(su_pulse_finder_t)), goto fail);
  SU_TRYCATCH(coef = malloc(sizeof(SUFLOAT) * len), goto fail);

  for (i = 0; i < len; ++i)
    coef[i] = peak;

  SU_TRYCATCH(
      su_iir_filt_init(
          &new->corr,
          0,    /* y_size */
          NULL, /* y_coef */
          len,  /* x_size */
          coef  /* x_coef */),
      goto fail);

  new->base     = base;
  new->peak_thr = (peak - base) * (peak - base) * len * (1 - tolerance);
  new->length   = len;

  new->time_tolerance  = len * tolerance;

  return new;

fail:
  if (!ok) {
    if (new != NULL)
      su_pulse_finder_destroy(new);
    new = NULL;
  }

  if (coef != NULL)
    free(coef);

  return new;
}

SUBOOL
su_pulse_finder_feed(su_pulse_finder_t *self, SUFLOAT x)
{
  SUFLOAT y;
  SUBOOL match;
  SUBOOL found = SU_FALSE;

  x -= self->base;

  y = SU_C_REAL(su_iir_filt_feed(&self->corr, x));

  match = y > self->peak_thr;

  if (self->present) {
    if (!match) {
      if (self->duration <= self->time_tolerance) {
        self->rel_pos = -self->length - self->accum / self->w_accum;
        found = SU_TRUE;
      }
    } else {
      self->accum   += x * self->duration++;
      self->w_accum += x;
    }
  } else if (match) {
    self->duration = 0;
    self->w_accum = x;
    self->accum = 0;
  }

  self->present = match;

  return found;
}

SUFLOAT
su_pulse_finder_get_pos(const su_pulse_finder_t *self)
{
  return self->rel_pos;
}

void
su_pulse_finder_destroy(su_pulse_finder_t *self)
{
  su_iir_filt_finalize(&self->corr);

  free(self);
}

/***************************** TV Processor ***********************************/
void
su_tv_processor_params_pal(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->hsync_len  = SU_T2N_COUNT(samp_rate, 4.749e-6);
  self->vsync_len  = SU_T2N_COUNT(samp_rate, 2.375e-6);
  self->line_len   = SU_T2N_COUNT(samp_rate, 63.556e-6);
  self->max_lines  = 252;
  self->vsync_pulses  = 5;
}

void
su_tv_processor_params_ntsc(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->hsync_len  = SU_T2N_COUNT(samp_rate, 4e-6);
  self->vsync_len  = SU_T2N_COUNT(samp_rate, 2e-6);
  self->line_len   = SU_T2N_COUNT(samp_rate, 64e-6);
  self->max_lines  = 305;

  self->vsync_pulses  = 6;
}

su_tv_processor_t *
su_tv_processor_new(const struct sigutils_tv_processor_params *params)
{
  su_tv_processor_t *new = NULL;

  SU_TRYCATCH(new = calloc(1, sizeof(su_tv_processor_t)), goto fail);
  SU_TRYCATCH(su_tv_processor_set_params(new, params), goto fail);
  return new;

fail:
  if (new != NULL)
    su_tv_processor_destroy(new);

  return NULL;
}

SUBOOL
su_tv_processor_set_params(
    su_tv_processor_t *self,
    const struct sigutils_tv_processor_params *params)
{
  su_pulse_finder_t *hsync_finder = NULL;
  su_pulse_finder_t *vsync_finder = NULL;
  SUFLOAT *line_buffer = NULL;

  SU_TRYCATCH(
      hsync_finder = su_pulse_finder_new(
          params->black_level,
          params->sync_level,
          params->hsync_len,
          params->t_tol),
      goto fail);

  SU_TRYCATCH(
      vsync_finder = su_pulse_finder_new(
          params->black_level,
          params->sync_level,
          params->vsync_len,
          params->t_tol),
        goto fail);

  SU_TRYCATCH(
      line_buffer = malloc(sizeof(SUFLOAT) * params->line_len),
      goto fail);

  if (self->hsync_finder != NULL)
    su_pulse_finder_destroy(self->hsync_finder);

  if (self->vsync_finder != NULL)
    su_pulse_finder_destroy(self->vsync_finder);

  if (self->line_buffer != NULL)
    free(self->line_buffer);

  self->hsync_finder = hsync_finder;
  self->vsync_finder = vsync_finder;
  self->line_buffer  = line_buffer;

  self->line_ptr = 0;
  self->line_count = 0;
  self->field_parity = 0;
  self->state = SU_TV_PROCESSOR_STATE_FIND_VSYNC;

  self->params = *params;

  return SU_TRUE;

fail:
  if (hsync_finder != NULL)
    su_pulse_finder_destroy(hsync_finder);

  if (vsync_finder != NULL)
    su_pulse_finder_destroy(vsync_finder);

  if (line_buffer != NULL)
    free(line_buffer);

  return SU_FALSE;
}


/*
   * Algorithm:
   * During FIND_VSYNC
   *   Find HSYNC and VSYNC. Calculate DELTA = t(VSYNC) - t(HSYNC)
   *     DELTA ~ line: Even frame -> transition to HSYNC
   *     DELTA ~ line / 2: Odd frame -> transition to HSYNC
   *
   * During FIND_HSYNC
   *   Find HSYNC and VSYNC. On HSYNC found, deliver last line and reset ptr.
   *   On VSYNC found, reset line number and ptr.
   *   If line > max_lines, transition to FIND_VSYNC.
   */

SUBOOL
su_tv_processor_feed(su_tv_processor_t *self, SUFLOAT x)
{
  SUBOOL have_line = SU_FALSE;
  SUBOOL have_hsync;
  SUBOOL have_vsync;
  SUFLOAT delta;

  have_hsync = su_pulse_finder_feed(self->hsync_finder, x);
  have_vsync = su_pulse_finder_feed(self->vsync_finder, x);

  if (have_hsync) {
    self->last_hsync = self->ptr;
    self->last_hsync_add = su_pulse_finder_get_pos(self->hsync_finder);
    printf("Have HSYNC (ptr = %ld)\n", self->ptr);
  }

  if (have_vsync) {
    self->last_vsync = self->ptr;
    self->last_vsync_add = su_pulse_finder_get_pos(self->vsync_finder);

    delta = (self->last_vsync - self->last_hsync)
        - (self->last_vsync_add - self->last_hsync_add);

    printf("Have VSYNC (ptr = %ld)\n", self->ptr);
    printf("DELTA: %g, line length: %d\n", delta, self->params.line_len);
  }

  return have_line;
}

const SUFLOAT *
su_tv_processor_get_line_data(const su_tv_processor_t *self)
{
  return self->line_buffer;
}

SUSCOUNT
su_tv_processor_get_line_size(const su_tv_processor_t *self)
{
  return self->line_ptr;
}

SUSCOUNT
su_tv_processor_get_line_number(const su_tv_processor_t *self)
{
  return 2 * self->line_count + self->field_parity;
}


void
su_tv_processor_destroy(su_tv_processor_t *self)
{
  if (self->hsync_finder != NULL)
    su_pulse_finder_destroy(self->hsync_finder);

  if (self->vsync_finder != NULL)
    su_pulse_finder_destroy(self->vsync_finder);

  if (self->line_buffer != NULL)
    free(self->line_buffer);

  if (self != NULL)
    free(self);
}
