/*

  Copyright (C) 2020 Gonzalo José Carracedo Carballal

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

#define SU_LOG_DOMAIN "tvproc"

#include <sigutils/tvproc.h>

#include <string.h>

#include <sigutils/log.h>
#include <sigutils/sampling.h>

/***************************** Pulse finder ***********************************/
SU_INSTANCER(
    su_pulse_finder,
    SUFLOAT base,
    SUFLOAT peak,
    SUSCOUNT len,
    SUFLOAT tolerance)
{
  su_pulse_finder_t *new = NULL;
  SUFLOAT *coef = NULL;
  unsigned int i;
  SUBOOL ok = SU_FALSE;

  SU_ALLOCATE_FAIL(new, su_pulse_finder_t);
  SU_ALLOCATE_MANY_FAIL(coef, len, SUFLOAT);

  for (i = 0; i < len; ++i)
    coef[i] = peak - base;

  SU_CONSTRUCT_FAIL(
      su_iir_filt,
      &new->corr,
      0,    /* y_size */
      NULL, /* y_coef */
      len,  /* x_size */
      coef /* x_coef */);

  new->base = base;
  new->peak_thr = (peak - base) * (peak - base) * len *(1 - tolerance);
  new->length = len;

  new->time_tolerance = len *(1 - tolerance);

  return new;

fail:
  if (!ok) {
    if (new != NULL)
      SU_DISPOSE(su_pulse_finder, new);
    new = NULL;
  }

  if (coef != NULL)
    free(coef);

  return new;
}

SU_METHOD(su_pulse_finder, SUBOOL, feed, SUFLOAT x)
{
  SUFLOAT y;
  SUBOOL match;
  SUBOOL found = SU_FALSE;

  x -= self->base;

  y = SU_C_REAL(su_iir_filt_feed(&self->corr, x));

  match = y > self->peak_thr;

  self->last_y = y;

  if (self->present) {
    if (!match) {
      if (self->duration <= self->time_tolerance) {
        self->rel_pos = -self->accum / self->w_accum + (SUFLOAT)self->length;
        found = SU_TRUE;
      }
    } else {
      self->accum += y * self->duration++;
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

SU_GETTER(su_pulse_finder, SUFLOAT, get_pos)
{
  return self->rel_pos;
}

SU_COLLECTOR(su_pulse_finder)
{
  SU_DESTRUCT(su_iir_filt, &self->corr);

  free(self);
}

/***************************** TV Processor ***********************************/
void
su_tv_processor_params_ntsc(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->enable_sync   = SU_TRUE;
  self->reverse       = SU_FALSE;
  self->interlace     = SU_TRUE;
  self->enable_agc    = SU_TRUE;
  self->x_off         = 0;
  self->dominance     = SU_TRUE;
  self->frame_lines   = 525;
  self->frame_spacing = 0.0;
  
  self->enable_comb   = SU_TRUE;
  self->comb_reverse  = SU_FALSE;

  self->hsync_len    = SU_T2N_FLOAT(samp_rate, 4.749e-6);
  self->vsync_len    = SU_T2N_FLOAT(samp_rate, 2.375e-6);
  self->line_len     = SU_T2N_FLOAT(samp_rate, 63.556e-6);
  self->vsync_odd_trigger = 6; /* VSYNC counter to trigger vertical sync */
  
  self->t_tol = 1e-1; /* Timing error tolerance */
  self->l_tol = 1e-1; /* Level error tolerance */
  self->g_tol = 1e-1; /* Geometry adjustment tolerance */

  self->hsync_huge_err = .25;
  self->hsync_max_err = 1e-2;  /* Maximum time error for hsync */
  self->hsync_min_err = .5e-2; /* Minimum time error for hsync */

  self->hsync_len_tau = 9.5; /* Time constant for hsync length adjust */
  self->line_len_tau = 1e3;  /* Time constant for line length estimation */
  self->agc_tau = 1e-5;      /* Time constant for AGC adjustment (frames) */

  self->hsync_fast_track_tau =
      9.5; /* Time constant for horizontal adjustment */
  self->hsync_slow_track_tau =
      1e3; /* Time constant for horizontal adjustment */
}

void
su_tv_processor_params_pal(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate)
{
  self->enable_sync   = SU_TRUE;
  self->reverse       = SU_FALSE;
  self->interlace     = SU_TRUE;
  self->enable_agc    = SU_TRUE;
  self->x_off         = 0;
  self->dominance     = SU_TRUE;
  self->frame_lines   = 625;
  self->frame_spacing = 0.0;

  self->enable_comb  = SU_TRUE;
  self->comb_reverse = SU_FALSE;

  self->hsync_len    = SU_T2N_FLOAT(samp_rate, 4e-6);
  self->vsync_len    = SU_T2N_FLOAT(samp_rate, 2e-6);
  self->line_len     = SU_T2N_FLOAT(samp_rate, 64e-6);
  self->vsync_odd_trigger = 5; /* VSYNC counter to trigger vertical sync */

  self->t_tol = 1e-1; /* Timing error tolerance */
  self->l_tol = 1e-1; /* Level error tolerance */
  self->g_tol = 1e-1; /* Geometry adjustment tolerance */

  self->hsync_huge_err = .25;
  self->hsync_max_err = 1e-2;  /* Maximum time error for hsync */
  self->hsync_min_err = .5e-2; /* Minimum time error for hsync */

  self->hsync_len_tau = 9.5; /* Time constant for hsync length adjust */
  self->line_len_tau = 1e3;  /* Time constant for line length estimation */
  self->agc_tau = 1e-5;      /* Time constant for AGC adjustment (frames) */

  self->hsync_fast_track_tau =
      9.5; /* Time constant for horizontal adjustment */
  self->hsync_slow_track_tau =
      1e3; /* Time constant for horizontal adjustment */
}

SU_INSTANCER(
    su_tv_frame_buffer,
    const struct sigutils_tv_processor_params *params)
{
  su_tv_frame_buffer_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_tv_frame_buffer_t);

  new->width = SU_CEIL(params->line_len);
  new->height = params->frame_lines;

  /*
   * Allocate extra line for fractional line numbers.
   */
  SU_ALLOCATE_MANY_FAIL(new->buffer, new->width * (new->height + 1), SUFLOAT);

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_tv_frame_buffer, new);

  return NULL;
}

SU_COPY_INSTANCER(su_tv_frame_buffer)
{
  su_tv_frame_buffer_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_tv_frame_buffer_t);

  new->width = self->width;
  new->height = self->height;

  SU_ALLOCATE_MANY_FAIL(new->buffer, new->width *new->height, SUFLOAT);

  memcpy(new->buffer, self->buffer, sizeof(SUFLOAT) * new->width *new->height);

  return new;

fail:
  if (new != NULL)
    SU_DISPOSE(su_tv_frame_buffer, new);

  return NULL;
}

SU_COLLECTOR(su_tv_frame_buffer)
{
  if (self->buffer != NULL)
    free(self->buffer);

  free(self);
}

SU_INSTANCER(su_tv_processor, const struct sigutils_tv_processor_params *params)
{
  su_tv_processor_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_tv_processor_t);

  SU_TRYCATCH(su_tv_processor_set_params(new, params), goto fail);

  new->agc_gain = 1;

  return new;

fail:
  if (new != NULL)
    su_tv_processor_destroy(new);

  return NULL;
}

SUINLINE
SU_METHOD(su_tv_processor, void, swap_field)
{
  SUBOOL spacing = SU_TRUE;

  if (self->params.interlace) {
    spacing = self->field_parity;
    self->field_parity = !self->field_parity;
    self->field_lines = self->params.frame_lines / 2 + self->field_parity;
  } else {
    self->field_lines = self->params.frame_lines;
  }

  if (spacing) {
    self->field_line_due += self->params.frame_spacing;
    if (self->field_line_due >= 1) {
      self->field_line_due -= 1.;
      ++self->field_lines;
    }
  }
}

SUINLINE
SU_GETTER(su_tv_processor, SUSCOUNT, get_line)
{
  if (self->params.interlace)
    return 2 * self->field_y + !self->field_parity;
  else
    return self->field_y;
}

SU_METHOD(
    su_tv_processor,
    SUBOOL,
    set_params,
    const struct sigutils_tv_processor_params *params)
{
  SUFLOAT *line_buffer = NULL;
  SUFLOAT *tmp = NULL;

  SUSCOUNT delay_line_len = SU_CEIL(params->line_len);
  SUBOOL ok = SU_FALSE;

  SU_TRY_FAIL(params->line_len >= 1);
  SU_TRY_FAIL(params->frame_lines >= 1);

  SU_TRY_FAIL(!params->enable_sync || params->hsync_len >= 1);
  SU_TRY_FAIL(!params->enable_sync || params->vsync_len >= 1);

  /* Reset comb filter */
  self->delay_line_len = delay_line_len;

  if (params->enable_comb) {
    if (self->delay_line_len != delay_line_len || line_buffer == NULL) {
      SU_TRY_FAIL(tmp = realloc(line_buffer, sizeof(SUFLOAT) * delay_line_len));

      line_buffer = tmp;

      if (self->delay_line == NULL) {
        memset(line_buffer, 0, sizeof(SUFLOAT) * delay_line_len);
      } else if (delay_line_len > self->delay_line_len) {
        memset(
            line_buffer + self->delay_line_len,
            0,
            sizeof(SUFLOAT) * (delay_line_len - self->delay_line_len));
      }
    } else {
      line_buffer = self->delay_line;
    }
  } else {
    if (self->delay_line != NULL)
      free(self->delay_line);
    line_buffer = NULL;
  }

  self->delay_line = line_buffer;
  line_buffer = NULL;

  self->params = *params;
  self->state = SU_TV_PROCESSOR_SEARCH;

#if 0
  /* Reset coordinates */
  self->field_x        = 0;
  self->field_x_dec    = 0;
  self->field_y        = 0;
  self->field_parity   = SU_TRUE;
  self->field_prev_ptr = 0;
  su_tv_processor_swap_field(self);
#endif

  /* Reset AGC state */
  if (!SU_VALID(self->agc_gain)) {
    self->agc_gain = 1;
    self->agc_line_max = 0;
    self->agc_accum = 0;
    self->agc_lines = 0;
  }

  /* Reset line due */
  self->field_line_due   = 0.;

  /* Reset pulse filter state */
  self->pulse_x = 0;

  /* Reset pulse finder state */
  self->sync_found = SU_FALSE;
  self->sync_start = 0;

  /* Reset HSYNC detector state */
  self->last_hsync = 0;
  self->have_last_hsync = SU_FALSE;
  self->est_hsync_len = params->hsync_len;

  /* Reset VSYNC detector state */
  self->last_frame = 0;
  self->last_vsync = 0;
  self->hsync_slow_track = SU_FALSE;

  /* Reset line estimations */
  self->est_line_len = params->line_len;
  self->est_line_len_accum = 0;
  self->est_line_len_count = 0;

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
   * Is a one pole IIR low pass filter. The frequency response of this
   * filter is:
   *                     1
   * H[z] = --------------------------
   *         1 - (1 - alpha) * z^-1
   *
   * There is a well known relationship between alpha and the time it takes
   * for the input to settle after an abrupt change in amplitude. In
   * particular, it is customary to express 1 - alpha in terms of a
   * time constant tau
   *
   *     1 - alpha = exp(-1 / tau)
   *
   * Tau tells you the amount of samples it takes for the output to settle
   * to 1 - 1/e =  63.2% of the new amplitude, starting from the old one.
   * Waiting 5 tau samples ensures that the output is stabilized to the
   * 99.3%. This provides a nice way to estimate the amplitude of the inverted
   * composite signal based on the sync amplitude.
   *
   * Let's assume HSYNC = 5 tau --> tau = HSYNC / 5. Hence
   *
   *   1 - alpha = exp(-5 / HSYNC) --> alpha = 1 - exp(-5 / HSYNC)
   */

  /* Data precalculation */
  self->pulse_alpha = SU_SPLPF_ALPHA(params->hsync_len / 5);
  self->agc_alpha = SU_SPLPF_ALPHA(params->agc_tau);
  self->hsync_len_alpha = SU_SPLPF_ALPHA(params->hsync_len_tau);
  self->hsync_slow_track_alpha = SU_SPLPF_ALPHA(params->hsync_slow_track_tau);
  self->hsync_fast_track_alpha = SU_SPLPF_ALPHA(params->hsync_fast_track_tau);
  self->line_len_alpha = SU_SPLPF_ALPHA(params->line_len_tau);
  
  ok = SU_TRUE;

fail:
  if (line_buffer != NULL)
    free(line_buffer);

  return ok;
}

SUINLINE
SU_GETTER(
    su_tv_processor,
    SUBOOL,
    frame_buffer_is_valid,
    const struct sigutils_tv_frame_buffer *fb)
{
  return fb->width == self->delay_line_len
         && fb->height == self->params.frame_lines;
}

SUINLINE
SU_METHOD(su_tv_processor, su_tv_frame_buffer_t *, take_from_pool)
{
  struct sigutils_tv_frame_buffer *this = NULL;

  while (self->free_pool != NULL) {
    this = self->free_pool;
    self->free_pool = this->next;

    if (su_tv_processor_frame_buffer_is_valid(self, this))
      break;

    SU_DISPOSE(su_tv_frame_buffer, this);
    this = NULL;
  }

  return this;
}

SUINLINE
SU_METHOD(su_tv_processor, void, return_to_pool, su_tv_frame_buffer_t *this)
{
  this->next = self->free_pool;
  self->free_pool = this;
}

SUPRIVATE
SU_METHOD(su_tv_processor, SUBOOL, assert_current_frame)
{
  if (self->current != NULL) {
    if (!su_tv_processor_frame_buffer_is_valid(self, self->current)) {
      su_tv_processor_return_to_pool(self, self->current);
      self->current = NULL;
    }
  }

  if (self->current == NULL) {
    if ((self->current = su_tv_processor_take_from_pool(self)) == NULL) {
      SU_TRYCATCH(
          self->current = su_tv_frame_buffer_new(&self->params),
          return SU_FALSE);
    }
  }

  return SU_TRUE;
}

SU_METHOD(su_tv_processor, su_tv_frame_buffer_t *, take_frame)
{
  struct sigutils_tv_frame_buffer *curr = self->current;

  self->current = NULL;

  return curr;
}

SU_METHOD(su_tv_processor, void, return_frame, su_tv_frame_buffer_t *fb)
{
  su_tv_processor_return_to_pool(self, fb);
}

SUINLINE
SU_METHOD(su_tv_processor, SUFLOAT, comb_filter_feed, SUFLOAT x)
{
  SUFLOAT prev_x;

  if (self->delay_line != NULL) {
    if (self->delay_line_ptr >= self->delay_line_len)
      self->delay_line_ptr %= self->delay_line_len;

    prev_x = self->delay_line[self->delay_line_ptr];

    if (self->params.comb_reverse)
      prev_x = -prev_x;

    self->delay_line[self->delay_line_ptr] = x;

    ++self->delay_line_ptr;

    x = .5 * (x + prev_x);
  }

  return x;
}

SUINLINE
SU_METHOD(su_tv_processor, SUFLOAT, pulse_filter_feed, SUFLOAT x)
{
  self->pulse_x += self->pulse_alpha * (x - self->pulse_x);

  return self->pulse_x;
}

SUINLINE
SU_METHOD(su_tv_processor, void, line_agc_feed, SUFLOAT x)
{
  if (x > self->agc_line_max)
    self->agc_line_max = x;
}

SUINLINE
SU_METHOD(su_tv_processor, void, line_agc_commit)
{
  self->agc_accum += self->agc_line_max;
  ++self->agc_lines;
  self->agc_line_max = 0;
}

SUINLINE
SU_METHOD(su_tv_processor, void, line_agc_update_gain)
{
  if (self->agc_lines > 10) {
    self->agc_gain += (self->agc_lines / self->agc_accum - self->agc_gain);
    self->agc_accum = 0;
    self->agc_lines = 0;
  }
}

SUINLINE
SU_GETTER(su_tv_processor, SUFLOAT, get_field_x)
{
  return self->field_x + self->field_x_dec;
}

SUINLINE
SU_METHOD(su_tv_processor, void, set_field_x, SUFLOAT xf)
{
  self->field_x = SU_FLOOR(xf);
  self->field_x_dec = xf - self->field_x;
}

SUINLINE
SU_METHOD(su_tv_processor, void, measure_line_len)
{
  SUSCOUNT new_line_len;

  if (self->have_last_hsync) {
    new_line_len = self->sync_start - self->last_hsync;

    if (sufreleq(new_line_len, self->params.line_len, self->params.g_tol)) {
      self->est_line_len_accum += new_line_len;
      ++self->est_line_len_count;
    }
  }

  self->have_last_hsync = SU_TRUE;
  self->last_hsync = self->sync_start;
}

SUINLINE
SU_METHOD(su_tv_processor, void, estimate_line_len)
{
  if (self->est_line_len_count > 0 && self->field_parity) {
    self->est_line_len += self->line_len_alpha
                          * (self->est_line_len_accum / self->est_line_len_count
                             - self->est_line_len);
    self->est_line_len_count = 0;
    self->est_line_len_accum = 0;
  }
}

/*
 * HSYNC pulses: these are particularly important, as vertical
 * synchronization relies on a properly working horizontal
 * synchronization.
 *
 * 1. Estimate the TRUE hsync period, needed for centering and more
 *    accurate hsync detection.
 * 2. Scroll the picture to the right X offset, smoothly. We use the
 *    traditional single pole IIR low pass filter in order to achieve
 *    this. We do this only for the first pulse we find.
 */
SUINLINE
SU_METHOD(su_tv_processor, void, do_hsync, SUSCOUNT hsync_len)
{
  SUFLOAT xf = su_tv_processor_get_field_x(self);
  SUFLOAT xf_offset =
      self->params.hsync_len / 2 + self->params.x_off * self->est_line_len;
  SUFLOAT xf_error = xf_offset - xf;

  /* 1. Improve HSYNC length estimation */
  self->est_hsync_len +=
      self->hsync_len_alpha * (hsync_len - self->est_hsync_len);

  /* 2. Horizontal offset ajustment. */
  if (SU_ABS(xf_error / self->est_line_len) > self->params.hsync_max_err) {
    self->state = SU_TV_PROCESSOR_SEARCH;

    if (SU_ABS(xf_error / self->est_line_len) > self->params.hsync_huge_err)
      self->hsync_slow_track = SU_FALSE;
  }

  if (self->state == SU_TV_PROCESSOR_SEARCH) {
    xf_error = xf_offset - xf;

    /* Derived from the max lines and off tau */
    if (self->hsync_slow_track)
      xf += self->hsync_slow_track_alpha * xf_error;
    else
      xf += self->hsync_fast_track_alpha * xf_error;

    su_tv_processor_set_field_x(self, xf);

    xf_error = xf_offset - xf;

    if (SU_ABS(xf_error / self->est_line_len) < self->params.hsync_min_err) {
      self->state = SU_TV_PROCESSOR_SYNCED;
      self->hsync_slow_track = SU_TRUE;
    }
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

SUINLINE
SU_METHOD(su_tv_processor, SUBOOL, do_vsync, SUSCOUNT vsync_len)
{
  SUSCOUNT last_hsync_age;
  SUSCOUNT last_vsync_age;
  SUFLOAT frame_len = self->est_line_len * self->params.frame_lines;

  SUFLOAT vsync_pos;
  SUBOOL vsync_forced = SU_FALSE;

  last_hsync_age = self->ptr - self->last_hsync;
  last_vsync_age = self->ptr - self->last_vsync;

  vsync_pos = SU_MOD(last_hsync_age, self->est_line_len);

  if (sufreleq(vsync_pos, self->est_line_len / 2, 2 * self->params.t_tol)
      && last_vsync_age > frame_len / 4) {
    /*
     * First oddly separated pulse seen in a while.
     * This marks the beginning of a vsync pulse train
     */
    self->vsync_counter = 1;
  } else if (
      sufreleq(last_vsync_age, self->est_line_len / 2, 2 * self->params.t_tol)
      && self->vsync_counter > 0) {
    /*
     * Last vsync found half a line ago, and we stared counting. Increase
     * the counter.
     */
    if (++self->vsync_counter == self->params.vsync_odd_trigger) {
      if (self->field_parity)
        su_tv_processor_swap_field(self);
      self->field_y = self->field_lines - 1;
      vsync_forced = SU_TRUE;
    }
  } else {
    /*
     * None of the above. Reset the pulse train.
     */
    self->vsync_counter = 0;
  }

  self->last_vsync = self->ptr;

  return vsync_forced;
}

SUINLINE
SU_METHOD(su_tv_processor, void, sync_feed, SUFLOAT pulse_x)
{
  SUBOOL pulse_trigger_up;
  SUBOOL pulse_trigger_down;

  SUSCOUNT sync_len;

  pulse_x *= self->agc_gain;

  pulse_trigger_up = pulse_x > (1 - self->params.l_tol);
  pulse_trigger_down = pulse_x < (1 - self->params.l_tol);

  if (!self->sync_found) {
    if (pulse_trigger_up) { /* SYNC PULSE START */
      self->sync_found = SU_TRUE;
      self->sync_start = self->ptr;
    }
  } else {
    if (pulse_trigger_down) { /* SYNC PULSE END */
      self->sync_found = SU_FALSE;
      sync_len = self->ptr - self->sync_start;

      /*
       * We now proceed to compare the pulse length to any known
       * pulse length (hsync and vsync).
       */
      if (sufreleq(sync_len, self->params.hsync_len, self->params.t_tol)) {
        su_tv_processor_measure_line_len(self);
        su_tv_processor_do_hsync(self, sync_len);
      } else {
        self->have_last_hsync = SU_FALSE;
      }

      if (sufreleq(sync_len, self->params.vsync_len, 2 * self->params.t_tol))
        if (!self->frame_has_vsync)
          if (su_tv_processor_do_vsync(self, sync_len))
            self->frame_has_vsync = SU_TRUE;
    }
  }
}

SUINLINE
SU_METHOD(su_tv_processor, SUBOOL, frame_feed, SUFLOAT x)
{
  SUSCOUNT line;
  SUSCOUNT p;
  SUFLOAT xf;
  SUFLOAT beta = self->field_x_dec;
  SUFLOAT value =
      self->params.reverse ? self->agc_gain * x : (1 - self->agc_gain * x);
  SUBOOL have_line = SU_FALSE;

  line = su_tv_processor_get_line(self);

  if (self->field_x < self->delay_line_len && line < self->params.frame_lines) {
    p = line * self->delay_line_len + self->field_x;

    if (self->field_prev_ptr < p)
      self->current->buffer[self->field_prev_ptr] += (1 - beta) * value;

    self->current->buffer[p] = beta * value;
    self->field_prev_ptr = p;
  }

  ++self->field_x;

  xf = su_tv_processor_get_field_x(self);

  if (xf >= self->est_line_len) {
    if (xf < 2 * self->est_line_len)
      su_tv_processor_set_field_x(self, xf - self->est_line_len);
    else
      su_tv_processor_set_field_x(
          self,
          xf - self->est_line_len * SU_FLOOR(xf / self->est_line_len));

    have_line = SU_TRUE;

    if (++self->field_y >= self->field_lines) {
      su_tv_processor_swap_field(self);
      su_tv_processor_estimate_line_len(self);
      self->field_y = 0;
      self->field_complete = SU_TRUE;
      self->frame_has_vsync = SU_FALSE;
    } else {
      self->field_complete = SU_FALSE;
    }
  }

  return have_line;
}

SU_METHOD(su_tv_processor, SUBOOL, feed, SUFLOAT x)
{
  SUBOOL have_frame = SU_FALSE;
  SUFLOAT pulse_x;

  SU_TRYCATCH(su_tv_processor_assert_current_frame(self), return SU_FALSE);

  pulse_x = su_tv_processor_pulse_filter_feed(self, x);

  x = su_tv_processor_comb_filter_feed(self, x);

  if (self->params.enable_agc)
    su_tv_processor_line_agc_feed(self, pulse_x);

  if (self->params.enable_sync)
    su_tv_processor_sync_feed(self, pulse_x);

  if (su_tv_processor_frame_feed(self, x)) {
    if (self->params.enable_agc)
      su_tv_processor_line_agc_commit(self);

    if (self->field_complete) {
      have_frame = !self->params.interlace
                   || self->field_parity == !!self->params.dominance;
      if (have_frame)
        self->last_frame = self->ptr;
      if (self->params.enable_agc)
        su_tv_processor_line_agc_update_gain(self);
    }
  }

  ++self->ptr;

  return have_frame;
}

SU_COLLECTOR(su_tv_processor)
{
  struct sigutils_tv_frame_buffer *frame = NULL;

  while ((frame = su_tv_processor_take_from_pool(self)) != NULL)
    SU_DISPOSE(su_tv_frame_buffer, frame);

  if (self->current != NULL)
    SU_DISPOSE(su_tv_frame_buffer, self->current);

  if (self->delay_line != NULL)
    free(self->delay_line);

  if (self != NULL)
    free(self);
}
