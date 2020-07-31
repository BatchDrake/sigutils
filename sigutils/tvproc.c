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
    coef[i] = peak - base;

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

  new->time_tolerance  = len * (1 - tolerance);

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

  /*
  if (y > 0)
    printf("  %3.1lf%%\n", 100 * y / self->peak_thr);
*/

  match = y > self->peak_thr;

  self->last_y = y;

  if (self->present) {
    if (!match) {
      if (self->duration <= self->time_tolerance) {
        self->rel_pos = - self->accum / self->w_accum + (SUFLOAT) self->length;
        found = SU_TRUE;
      }
    } else {
      self->accum   += y * self->duration++;
      self->w_accum += y;
    }
  } else if (match) {
    self->duration = 0;
    self->w_accum = y;
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
su_tv_processor_params_ntsc(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->hsync_len  = SU_T2N_COUNT(samp_rate, 4.749e-6);
  self->vsync_len  = SU_T2N_COUNT(samp_rate, 2.375e-6);
  self->line_len   = SU_T2N_COUNT(samp_rate, 63.556e-6);
  self->frame_len  = SU_T2N_COUNT(samp_rate, 1 / 59.94);

  self->enable_comb   = SU_TRUE;
  self->max_lines     = 252;
  self->vsync_pulses  = 6;

  self->sync_level    = 0;
  self->black_level   = .27;
  self->white_level   = 1;
}

void
su_tv_processor_params_pal(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->hsync_len  = SU_T2N_COUNT(samp_rate, 4e-6);
  self->vsync_len  = SU_T2N_COUNT(samp_rate, 2e-6);
  self->line_len   = SU_T2N_COUNT(samp_rate, 64e-6);
  self->frame_len  = SU_T2N_COUNT(samp_rate, 1 / 50.);

  self->enable_comb  = SU_TRUE;
  self->max_lines    = 312;
  self->vsync_pulses = 5;

  self->sync_level    = 0;
  self->black_level   = .27;
  self->white_level   = 1;
}


struct sigutils_tv_frame_buffer *
su_tv_frame_buffer_new(const struct sigutils_tv_processor_params *params)
{
  struct sigutils_tv_frame_buffer *new = NULL;

  SU_TRYCATCH(
      new = calloc(1, sizeof (struct sigutils_tv_frame_buffer)),
      goto fail);

  new->width  = params->line_len;
  new->height = 2 * params->max_lines + 1;

  SU_TRYCATCH(
      new->buffer = calloc(sizeof(SUFLOAT), new->width * new->height),
      goto fail);

  return new;

fail:
  if (new != NULL)
    su_tv_frame_buffer_destroy(new);

  return NULL;
}

void
su_tv_frame_buffer_destroy(struct sigutils_tv_frame_buffer *self)
{
  if (self->buffer != NULL)
    free(self->buffer);

  free(self);
}

su_tv_processor_t *
su_tv_processor_new(const struct sigutils_tv_processor_params *params)
{
  su_tv_processor_t *new = NULL;

  SU_TRYCATCH(new = calloc(1, sizeof(su_tv_processor_t)), goto fail);
  SU_TRYCATCH(su_tv_processor_set_params(new, params), goto fail);

  new->agc_gain = 1;

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

  if (params->enable_comb) {
    SU_TRYCATCH(
        line_buffer = calloc(sizeof(SUFLOAT), params->line_len),
        goto fail);
  }

  if (self->hsync_finder != NULL)
    su_pulse_finder_destroy(self->hsync_finder);

  if (self->vsync_finder != NULL)
    su_pulse_finder_destroy(self->vsync_finder);

  if (self->delay_line != NULL)
    free(self->delay_line);

  self->hsync_finder = hsync_finder;
  self->vsync_finder = vsync_finder;

  self->field_x = 0;
  self->field_y = 0;
  self->field_parity = 0;
  self->state = SU_TV_PROCESSOR_SEARCH;
  self->delay_line = line_buffer;
  self->true_line_len = params->line_len;
  self->true_hsync_len = params->hsync_len;
  self->have_last_hsync = SU_FALSE;



  /*
   * Another good reminder on single pole IIR low pass filters
   * ----------------------------------------------------------------
   * The following well-known LPF:
   *
   *     y = alpha * x + (1 - alpha) * y
   *
   * Also written like this:
   *
   *     y += alpha * (x - y)
   *
   * Is a one pole IIR low pass filter. The frequency response of
   * this filter is:
   *                     1
   * H[z] = --------------------------
   *         1 - (1 - alpha) * z^-1
   *
   * There is a well knwon relationship between alpha and the time it takes
   * for the input to settle after an abrupt change in amplitude. In
   * particular, it is customary to express 1 - alpha in terms of a
   * time constant tau
   *
   *     1 - alpha = exp(-1 / tau)
   *
   * Tau tells you the amount of samples it takes for the output to settle
   * to 1 - 1/e =  63.2% of the new amplitude, starting from the old one.
   * Waiting 5 tau samples, ensures that the output is stabilized to the
   * 99.3%. This is ideal to estimate the amplitude of the inverted composite
   * signal based on the sync amplitude.
   *
   * Let's assume HSYNC = 5 tau --> tau = HSYNC / 5. Hence
   *
   *   1 - alpha = exp(-5 / HSYNC) --> alpha = 1 - exp(-5 / HSYNC)
   */

  self->pulse_alpha = 1 - SU_EXP(-5. / params->hsync_len);

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

SUINLINE SUBOOL
su_tv_processor_frame_buffer_is_valid(
    const su_tv_processor_t *self,
    const struct sigutils_tv_frame_buffer *fb)
{
  return fb->width == self->params.line_len
      && fb->height == (2 * self->params.max_lines + 1);
}

SUINLINE struct sigutils_tv_frame_buffer *
su_tv_processor_take_from_pool(su_tv_processor_t *self)
{
  struct sigutils_tv_frame_buffer *this = NULL;

  while (self->free_pool != NULL) {
    this = self->free_pool;
    self->free_pool = this->next;

    if (su_tv_processor_frame_buffer_is_valid(self, this))
      break;

    su_tv_frame_buffer_destroy(this);
    this = NULL;
  }

  return this;
}

SUINLINE void
su_tv_processor_return_to_pool(
    su_tv_processor_t *self,
    struct sigutils_tv_frame_buffer *this)
{
  this->next = self->free_pool;
  self->free_pool = this;
}

SUINLINE void
su_tv_processor_force_scan_reset(su_tv_processor_t *self)
{
  self->field_x = self->field_y = 0;
  self->delay_line_ptr = 0;
  self->field_parity = SU_FALSE;
}

SUPRIVATE SUBOOL
su_tv_processor_assert_current_frame(su_tv_processor_t *self)
{
  if (self->current != NULL) {
    if (!su_tv_processor_frame_buffer_is_valid(self, self->current)) {
      su_tv_processor_force_scan_reset(self);
      su_tv_processor_return_to_pool(self, self->current);
      self->current = NULL;
    }
  }

  if (self->current == NULL) {
    su_tv_processor_force_scan_reset(self);
    if ((self->current = su_tv_processor_take_from_pool(self)) == NULL) {
      SU_TRYCATCH(
          self->current = su_tv_frame_buffer_new(&self->params),
          return SU_FALSE);
    }
  }

  return SU_TRUE;
}

struct sigutils_tv_frame_buffer *
su_tv_processor_take_frame(su_tv_processor_t *self)
{
  struct sigutils_tv_frame_buffer *curr = self->current;

  self->current = NULL;

  return curr;
}

void
su_tv_processor_return_frame(
    su_tv_processor_t *self,
    struct sigutils_tv_frame_buffer *fb)
{
  su_tv_processor_return_to_pool(self, fb);
}

SUINLINE SUFLOAT
su_tv_processor_comb_filter_feed(su_tv_processor_t *self, SUFLOAT x)
{
  SUFLOAT prev_x;

  if (self->delay_line != NULL) {
    prev_x = self->delay_line[self->delay_line_ptr];
    self->delay_line[self->delay_line_ptr] = x;
    if (++self->delay_line_ptr == self->params.line_len)
      self->delay_line_ptr = 0;

    x = .5 * (x + prev_x);
  }

  return x;
}

SUINLINE SUFLOAT
su_tv_processor_pulse_filter_feed(su_tv_processor_t *self, SUFLOAT x)
{
  self->pulse_x += self->pulse_alpha * (x - self->pulse_x);

  return self->pulse_x;
}

SUINLINE void
su_tv_processor_line_agc_feed(su_tv_processor_t *self, SUFLOAT x)
{
  if (x > self->agc_line_max)
    self->agc_line_max = x;
}

SUINLINE void
su_tv_processor_line_agc_commit(su_tv_processor_t *self)
{
  self->agc_accum += self->agc_line_max;
  ++self->agc_lines;
  self->agc_line_max = 0;
}

SUINLINE void
su_tv_processor_line_agc_update_gain(su_tv_processor_t *self)
{
  if (self->agc_lines > 10) {
    self->agc_gain += self->params.max_lines / self->agc_accum - self->agc_gain;
    self->agc_accum = 0;
  }
}

SUINLINE SUFLOAT
su_tv_processor_get_field_x(const su_tv_processor_t *self)
{
  return self->field_x + self->field_x_dec;
}

SUINLINE void
su_tv_processor_set_field_x(su_tv_processor_t *self, SUFLOAT xf)
{
  self->field_x = SU_FLOOR(xf);
  self->field_x_dec = xf - self->field_x;
}

/*
 * HSYNC pulses: these are particularly important, as vertical
 * synchronization relies on a properly working horizontal
 * synchronization.
 *
 * 1. Estimate the TRUE line period only if the line period is equal to
 *    the line period specified by the parameters, up to a tolerance and
 * 2. Estimate the TRUE hsync period, needed for centering and more
 *    accurate hsync detection.
 * 3. Scroll the picture to the right X offset, smoothly. We use the
 *    traditional single pole IIR low pass filter in order to achieve
 *    this. We do this only for the first pulse we find.
 */


SUINLINE void
su_tv_processor_do_hsync(su_tv_processor_t *self, SUSCOUNT hsync_len)
{
  SUSCOUNT new_line_len;
  SUFLOAT xf = su_tv_processor_get_field_x(self);
  SUFLOAT xf_offset = self->true_hsync_len / 2;
  SUFLOAT xf_error  = xf_offset - xf;

  /* 1. Improve estimation of line length */
  if (self->have_last_hsync) {
    new_line_len = self->sync_start - self->last_hsync;
    if (abs(new_line_len - self->params.line_len) < 50) {
      self->true_line_len_accum += new_line_len;
      ++self->true_line_len_count;
    }
  }

  /* 2. Improve HSYNC length estimation */
  self->true_hsync_len += .1 * (hsync_len - self->true_hsync_len);

  /* 2. Horizontal offset ajustment. */
  if (SU_ABS(xf_error) > 2 && !self->frame_synced) {
    self->state = SU_TV_PROCESSOR_SEARCH;
    self->hsync_corr_count = 0;
  }

  if (self->state == SU_TV_PROCESSOR_SEARCH) {
      SUFLOAT len;

      /* Correct X offset, smoothly */
      xf += .1 * (1 - SU_EXP(-5./100)) * xf_error;

      su_tv_processor_set_field_x(self, xf);

      xf_error  = xf_offset - xf;

      if (SU_ABS(xf_error) < 2 || ++self->hsync_corr_count >= 100)
        self->state = SU_TV_PROCESSOR_SYNCED;
  }
}


/*
 * VSYNC pulses: we only look for one distinctive pulse: the
 * first one that is an integer multiple and half of a line apart from
 * the last measured HSYNC. This marks the end of the odd field.
 *
 * We perform the following check: if the sync pulse is found more than
 * 10 lines earlier than expected, we resync vertically. We jump
 * straight to the last line, set the frame parity and mark the
 * frame as synced.
 */

SUINLINE SUBOOL
su_tv_processor_do_vsync(su_tv_processor_t *self, SUSCOUNT vsync_len)
{
  SUSCOUNT last_hsync_age;
  SUFLOAT  err_vsync_pos;
  SUSCOUNT vsync_pos;
  SUBOOL   vsync_forced = SU_FALSE;

  last_hsync_age = self->ptr - self->last_hsync;
  vsync_pos      = last_hsync_age % (SUSCOUNT) SU_FLOOR(self->true_line_len);
  err_vsync_pos  = 2 * SU_ABS(.5 - vsync_pos / self->true_line_len);

  if (self->true_line_len_count > 0) {
    self->true_line_len += 1e-1 *
        (self->true_line_len_accum / self->true_line_len_count -
            self->true_line_len);
    self->true_line_len_count = 0;
    self->true_line_len_accum = 0;
  }

  /*
   * TODO: mark frame as synced if the sync is found in
   * the right line.
   */
  if (err_vsync_pos <= .2
      && self->field_y < (self->params.max_lines - 10)
      && self->field_parity
      && self->ptr - self->last_vsync > 2 * self->true_line_len * self->params.max_lines) {
    self->field_y = self->params.max_lines - 1;
    self->field_parity = !self->field_parity;
    vsync_forced = SU_TRUE;
  }

  return vsync_forced;
}

SUINLINE void
su_tv_processor_sync_feed(
    su_tv_processor_t *self,
    SUFLOAT x,
    SUFLOAT pulse_x)
{

  SUBOOL   pulse_trigger_up;
  SUBOOL   pulse_trigger_down;

  SUSCOUNT sync_length;

  SUFLOAT  err_hsync;
  SUFLOAT  err_vsync;

  pulse_trigger_up   = self->agc_gain * pulse_x > 1 - .1;
  pulse_trigger_down = self->agc_gain * pulse_x < 1 - .1;

  if (!self->sync_found) {
    if (pulse_trigger_up) { /* SYNC PULSE START */
      self->sync_found = SU_TRUE;
      self->sync_start = self->ptr;
    }
  } else {
    if (pulse_trigger_down) { /* SYNC PULSE END */
      self->sync_found = SU_FALSE;
      sync_length      = self->ptr - self->sync_start;

      /*
       * We now proceed to compare the pulse length to any known
       * pulse length (hsync and vsync).
       */
      err_hsync = SU_ABS(1. - (SUFLOAT) sync_length / self->params.hsync_len);
      err_vsync = SU_ABS(1. - (SUFLOAT) sync_length / self->params.vsync_len);

      if (err_hsync <= .1) {
        su_tv_processor_do_hsync(self, sync_length);
        self->have_last_hsync = SU_TRUE;
        self->last_hsync = self->sync_start;
      } else {
        self->have_last_hsync = SU_FALSE;
      }

      if (err_vsync <= .2 && !self->frame_synced) {
        if (su_tv_processor_do_vsync(self, sync_length)) {
          self->last_vsync = self->sync_start;
          self->frame_synced = SU_TRUE;
        }
      }
    }
  }
}

SUINLINE SUBOOL
su_tv_processor_frame_feed(su_tv_processor_t *self, SUFLOAT x)
{
  SUSCOUNT line;
  SUSCOUNT p;
  SUFLOAT xf;
  SUFLOAT beta = self->field_x_dec;
  SUFLOAT value = 1 - self->agc_gain * x;
  SUBOOL have_line = SU_FALSE;

  line = 2 * self->field_y + self->field_parity;

  if (self->field_x < self->params.line_len
      && self->field_y < self->params.max_lines + !self->field_parity) {
    p = line * self->params.line_len + self->field_x;

    if (p > 0)
      self->current->buffer[p - 1] += (1 - beta) * value;

    self->current->buffer[p] = beta * value;
  }

  ++self->field_x;

  xf = su_tv_processor_get_field_x(self);

  if (xf >= self->true_line_len) {
    su_tv_processor_set_field_x(self, xf - self->true_line_len);
    have_line = SU_TRUE;

    if (++self->field_y >= self->params.max_lines + !self->field_parity) {
      self->field_y = 0;
      self->frame_synced = SU_FALSE;
      self->field_parity = !self->field_parity;
    }
  }

  return have_line;
}

SUBOOL
su_tv_processor_feed(su_tv_processor_t *self, SUFLOAT x)
{
  SUBOOL have_frame = SU_FALSE;
  SUFLOAT pulse_x;

  SU_TRYCATCH(su_tv_processor_assert_current_frame(self), return SU_FALSE);

  x = su_tv_processor_comb_filter_feed(self, x);

  pulse_x = su_tv_processor_pulse_filter_feed(self, x);

  su_tv_processor_line_agc_feed(self, pulse_x);

  su_tv_processor_sync_feed(self, x, pulse_x);

  if (su_tv_processor_frame_feed(self, x)) {
    su_tv_processor_line_agc_commit(self);

    if (self->field_y == 0) {
      su_tv_processor_line_agc_update_gain(self);
      have_frame = !self->field_parity;
    }
  }

  ++self->ptr;

  return have_frame;
}

SUSCOUNT
su_tv_processor_get_line_size(const su_tv_processor_t *self)
{
  return self->field_x;
}

void
su_tv_processor_destroy(su_tv_processor_t *self)
{
  struct sigutils_tv_frame_buffer *frame = NULL;

  while ((frame = su_tv_processor_take_from_pool(self)) != NULL)
    su_tv_frame_buffer_destroy(frame);

  if (self->current != NULL)
    su_tv_frame_buffer_destroy(self->current);

  if (self->hsync_finder != NULL)
    su_pulse_finder_destroy(self->hsync_finder);

  if (self->vsync_finder != NULL)
    su_pulse_finder_destroy(self->vsync_finder);

  if (self->delay_line != NULL)
    free(self->delay_line);

  if (self != NULL)
    free(self);
}
