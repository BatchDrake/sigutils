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

#include "tvproc.h"
#include <assert.h>
#include <string.h>

#include "log.h"
#include "sampling.h"

SU_CONSTRUCTOR(
  su_tv_pulse_separator,
  SUFLOAT sync_len,
  SUFLOAT line_len,
  SUFLOAT sync_level)
{
  SUBOOL ok = SU_FALSE;

  memset(self, 0, sizeof(su_tv_pulse_separator_t));

  self->search      = SU_TRUE;
  self->negative    = SU_TRUE;

  self->gain        = 1;
  self->line_len_orig = line_len;
  self->line_len = self->line_len_orig;
  self->sync_len    = sync_len;
  self->sync_level  = sync_level;

  self->sync_level_inv = 1 / sync_level;

  self->window_len  = 2 * sync_len;
  self->history_len = (line_len + self->window_len) * 1.5;
  
  SU_ALLOCATE_MANY(self->history, self->history_len, SUFLOAT);
  SU_ALLOCATE_MANY(self->shape, self->window_len, SUFLOAT);

  ok = SU_TRUE;

done:
  if (!ok)
    SU_DESTRUCT(su_tv_pulse_separator, self);
  
  return ok;

}

SU_METHOD(su_tv_pulse_separator, SUBOOL, feed, SUCOMPLEX x)
{
  SUFLOAT amp = SU_C_ABS(x), y0, y1;
  SUSCOUNT ptr;
  SUFLOAT pwr0, pwr1;
  SUFLOAT cmp;
  SUBOOL have_sync = SU_FALSE;

  int i;

  ++self->n;

  SU_SPLPF_FEED(self->amp_gain_y, amp, SU_SPLPF_ALPHA(self->sync_len * .5));

  ptr = self->ptr;

  /* This is the AGC impl */
  ++self->gain_samples;
  if (self->amp_gain_y > self->max_level) {
    self->max_level = self->amp_gain_y;
    self->max_loc   = ptr;
  }

  if (self->gain_samples > self->line_len) {
    if (self->gain_updates == 0 && !self->have_gain)
      self->line_gain = 1. / self->max_level;
    else
      SU_SPLPF_FEED(self->line_gain, 1. / self->max_level, SU_SPLPF_ALPHA(625));

    self->max_level = 0;
    ++self->gain_updates;
    self->gain_samples = 0;
  }

  if (self->gain_updates >= 625 / 2) {
    //printf("Gain estimated around %g (%d updates)\n", self->gain, self->gain_updates);
    self->gain_updates = 0;

    if (!self->have_gain)
      self->gain = self->line_gain;
    else
      SU_SPLPF_FEED(self->gain, self->line_gain, SU_SPLPF_ALPHA(25));

    //printf("GAIN UPDATE: %g\n", self->gain);

    self->have_gain = SU_TRUE;
  }

  if (!self->have_gain)
    goto done;

  //amp = self->amp_gain_y;
  amp = self->amp_gain_y * self->gain;

  self->history[ptr] = amp;

  ++self->ptr;
  if (self->ptr >= self->history_len)
    self->ptr = 0;
  

  if (self->fp == NULL) {
    self->fp = fopen("agc.raw", "wb");
  } else {
    fwrite(&amp, sizeof(SUFLOAT), 1, self->fp);
  }

  if (self->search) {
    SUSCOUNT p, q = 0;
    SUSCOUNT start = 0;
  
    fwrite(&amp, sizeof(SUFLOAT), 1, self->fp);
    /* Line full? */
    if (self->ptr == 0) {
      SUSCOUNT c;

      pwr0 = 0;
      pwr1 = 0;

      /* 
        We need to determine whether this looks like a sync. In order to do that,
        we compare the power in there with the power somewhere in the middle of
        the line.
      */
      start = self->history_len + self->max_loc - self->window_len / 2;
      if (start > self->history_len)
        start %= self->history_len;
      
      p = start;
      q = start + self->line_len / 2;
      if (q > self->history_len)
        q %= self->history_len;
      

      for (i = 0; i < self->window_len; ++i) {
        cmp = i >= (self->window_len - self->sync_len) / 2
              && i < (self->window_len + self->sync_len) / 2
              ? 1
              : -1;

        y0 = self->history[p++];
        y1 = self->history[q++];

        if (self->negative) {
          y0 = 1 - y0;
          y1 = 1 - y1;
        }

        y0 -= self->sync_level;
        y1 -= self->sync_level;

        if (y0 > 0)
          y0 = 0;
        
        if (y1 > 0)
          y1 = 0;

        y0 = -y0 * self->sync_level_inv - .5;
        y1 = -y1 * self->sync_level_inv - .5;


        self->shape[i] = y0;
        
        pwr0 += y0 * cmp;
        pwr1 += y1 * cmp;

        if (p > self->history_len)
          p = 0;
        
        if (q > self->history_len)
          q = 0;
      }

      /* Sounds plausible. Go ahead. */
      if (pwr1 < 0)
        pwr1 = 0;
      
      if (pwr0 > self->sync_len * .25 && pwr0 > 3 * pwr1) {
        printf("Definitely looks like a pulse to me (%g > 3 * %g)\n", pwr0, pwr1);
        self->search = SU_FALSE;
        self->sync_pwr = pwr0;
        self->next_sync = self->max_loc + self->line_len;

        if (self->next_sync > self->history_len)
          self->next_sync -=self->history_len;

        self->search_start = self->next_sync - self->window_len / 2;
        if (self->search_start < 0)
          self->search_start += self->history_len;

        self->search_end = self->search_start + self->window_len;
        if (self->search_end >= self->history_len)
          self->search_end -= self->history_len;

        printf("  PTR: %d\n", ptr);
        printf("  Search start: %d\n", self->search_start);
        printf("  Search end:   %d\n", self->search_end);
        self->max_level = 0;
        self->n_prev = self->n - ptr + self->max_loc;
      }
    }
  } else {
    SUBOOL in_window, transition;

    if (self->holdoff > 0) {
      --self->holdoff;
      in_window = SU_FALSE;
    } else {
      if (self->search_start < self->search_end) {
        /* Normal */
        in_window = self->search_start <= ptr && ptr < self->search_end;
      } else {
        /* Roll over */
        in_window = self->search_start <= ptr || ptr < self->search_end;
      }
    }

    SUFLOAT input;

    if (in_window)
      input = amp;
    else
      input = 0;

    fwrite(&input, sizeof(SUFLOAT), 1, self->fp);

    /* Measure pulse */
    transition = self->syncing != in_window;
    self->syncing = in_window;

    if (self->syncing) {
      SUFLOAT w;
      if (transition) {
        self->sync_accum = 0;
        self->sync_err = 0;
        self->sync_err_norm = 0;
        self->sync_count = 0;
      }

      i = self->sync_count - self->window_len / 2;

      cmp = self->shape[self->sync_count];

      y0 = amp;
      
      if (self->negative)
        y0 = 1 - y0;
      
      y0 -= self->sync_level;

      if (y0 > 0)
        y0 = 0;
      
      w  = -y0 * self->sync_level_inv;

      y0 = w - .5;

      pwr0 += y0 * cmp;

      self->sync_accum += pwr0;
      self->sync_err += (i + self->round_err) * w;
      self->sync_err_norm += w;

      ++self->sync_count;
    } else {
      if (transition) {
        SUFLOAT line_len;
        SUFLOAT search_start;
        SUFLOAT pwr = self->sync_pwr;

        if (self->sync_accum <= 0 || self->sync_err_norm <= 0) {
          if (++self->retries > 5) {
            printf("SYNC FINISHED: Out of sync, leaving after %d lines\n", self->line_count);
            self->line_count = 0;
            self->line_len = self->line_len_orig;
            self->search = SU_TRUE;
            goto done;
          } 

          line_len = self->line_len;
        } else {
          self->retries = 0;
          self->sync_err /= self->sync_err_norm;
          SU_SPLPF_FEED(self->sync_pwr, self->sync_accum, SU_SPLPF_ALPHA(self->window_len));

          if (!sufreleq(self->sync_pwr, pwr, .5)) {
            printf("SYNC MISSING: Out of sync, leaving after %d lines (pwr ratio is %g)\n", self->line_count, self->sync_pwr / pwr);
            self->line_count = 0;
            self->line_len = self->line_len_orig;
            self->search = SU_TRUE;
            goto done;
          }
          
          line_len = self->line_len + self->sync_err;
          if (line_len < 0)
            line_len += self->history_len;
          
          SU_SPLPF_FEED(self->line_len, line_len, SU_SPLPF_ALPHA(25 * 625));

          if (!sufreleq(self->line_len_orig, self->line_len, 1e-1)) {
            printf("Line len of %g !!!=== %g, out of sync\n", line_len, self->line_len_orig);
            self->line_len = self->line_len_orig;
            self->search = SU_TRUE;
            goto done;
          }
        }

        self->next_sync = self->next_sync + line_len;

        if (self->next_sync > self->history_len)
          self->next_sync -=self->history_len;

        search_start = self->next_sync - self->window_len / 2;
        self->search_start = search_start;
        self->round_err = search_start - self->search_start;

        if (self->search_start < 0)
          self->search_start += self->history_len;

        self->search_end = self->search_start + self->window_len;
        if (self->search_end >= self->history_len)
          self->search_end -= self->history_len;

        /*printf(
          "SYNC FINISHED, LINE LEN: %g (error %g samples, pwr_rate %g)\n",
          self->line_len,
          self->sync_err,
          self->sync_pwr / pwr);*/
        self->holdoff = self->window_len;
        self->n_prev = self->n;
        ++self->line_count;

        have_sync = SU_TRUE;
      }
    }
  }

done:
  return have_sync;
}

SU_DESTRUCTOR(su_tv_pulse_separator)
{
  if (self->history != NULL)
    free(self->history);

  if (self->shape != NULL)
    free(self->shape);
}

/***************************** Comb filter ************************************/
SU_CONSTRUCTOR(su_tv_comb_filter, SUSCOUNT len, SUBOOL negative)
{
  SUBOOL ok = SU_FALSE;

  memset(self, 0, sizeof(su_tv_comb_filter_t));

  self->history_len = len;
  self->negative = negative;
  
  SU_ALLOCATE_MANY(self->history, len, SUCOMPLEX);

  ok = SU_TRUE;

done:
  if (!ok)
    SU_DESTRUCT(su_tv_comb_filter, self);

  return ok;
}

SU_METHOD(su_tv_comb_filter, SUCOMPLEX, feed, SUCOMPLEX x)
{
  SUCOMPLEX prev;
  prev = self->history[self->ptr];
  self->history[self->ptr++] = x;

  if (self->ptr == self->history_len)
    self->ptr = 0;

  if (self->negative)
    return .5 * (x - prev);
  else
    return .5 * (x + prev);
}

SU_DESTRUCTOR(su_tv_comb_filter)
{
  if (self->history != NULL)
    free(self->history);
}

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
  self->enable_sync = SU_TRUE;
  self->reverse = SU_FALSE;
  self->interlace = SU_TRUE;
  self->enable_agc = SU_TRUE;
  self->x_off = 0;
  self->dominance = SU_TRUE;
  self->frame_lines = 525;

  self->enable_comb = SU_TRUE;
  self->comb_reverse = SU_FALSE;

  self->hsync_len = SU_T2N_FLOAT(samp_rate, 4.749e-6);
  self->vsync_len = SU_T2N_FLOAT(samp_rate, 2.375e-6);
  self->line_len = SU_T2N_FLOAT(samp_rate, 63.556e-6);
  self->vsync_odd_trigger = 6; /* VSYNC counter to trigger vertical sync */

  self->t_tol = 3e-1; /* Timing error tolerance */
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
  self->enable_sync = SU_TRUE;
  self->reverse = SU_FALSE;
  self->interlace = SU_TRUE;
  self->enable_agc = SU_TRUE;
  self->x_off = 0;
  self->dominance = SU_TRUE;
  self->frame_lines = 625;

  self->enable_comb = SU_TRUE;
  self->comb_reverse = SU_FALSE;

  self->hsync_len = SU_T2N_FLOAT(samp_rate, 4e-6);
  self->vsync_len = SU_T2N_FLOAT(samp_rate, 2e-6);
  self->line_len = SU_T2N_FLOAT(samp_rate, 64e-6);
  self->vsync_odd_trigger = 5; /* VSYNC counter to trigger vertical sync */


  self->enable_color = SU_TRUE;
  self->chroma_fhint = SU_ABS2NORM_FREQ(samp_rate, 4.43361875e6); /* hcps */
  self->chroma_burst_start = SU_T2N_FLOAT(samp_rate, 5.6e-6); 
  self->chroma_burst_len  = 20. / self->chroma_fhint;

  self->t_tol = 1e-1; /* Timing error tolerance */
  self->l_tol = 1e-1; /* Level error tolerance */
  self->g_tol = 1e-1; /* Geometry adjustment tolerance */

  self->hsync_huge_err = .25;
  self->hsync_max_err = 1e-2;  /* Maximum time error for hsync */
  self->hsync_min_err = .5e-2; /* Minimum time error for hsync */

  self->hsync_len_tau = 9.5; /* Time constant for hsync length adjust */
  self->line_len_tau = 1e3;  /* Time constant for line length estimation */
  self->agc_tau = 1e-5;      /* Time constant for AGC adjustment (frames) */
  self->chroma_res_tau = SU_T2N_FLOAT(samp_rate, 1.0e-6);

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

  SU_ALLOCATE_MANY_FAIL(new->buffer, new->width * new->height, SUFLOAT);
  SU_ALLOCATE_MANY_FAIL(new->chroma, new->width * new->height, SUCOMPLEX);

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

  SU_ALLOCATE_MANY_FAIL(new->buffer, new->width * new->height, SUFLOAT);
  SU_ALLOCATE_MANY_FAIL(new->chroma, new->width * new->height, SUCOMPLEX);

  memcpy(new->buffer, self->buffer, sizeof(SUFLOAT) * new->width * new->height);
  memcpy(new->chroma, self->chroma, sizeof(SUCOMPLEX) * new->width * new->height);

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

  if (self->chroma != NULL)
    free(self->chroma);
  
  free(self);
}


SUINLINE
SU_METHOD(su_tv_processor, void, swap_field)
{
  if (self->params.interlace) {
    self->field_parity = !self->field_parity;
    self->field_lines = self->params.frame_lines / 2 + self->field_parity;
  } else {
    self->field_lines = self->params.frame_lines;
  }
}

SU_INSTANCER(su_tv_processor, const struct sigutils_tv_processor_params *params)
{
  su_tv_processor_t *new = NULL;

  SU_ALLOCATE_FAIL(new, su_tv_processor_t);

  SU_TRYCATCH(su_tv_processor_set_params(new, params), goto fail);

  new->agc_gain = 1;
  new->chroma_gain = 1;

  su_pll_init(&new->chroma_pll, params->chroma_fhint, 0.0003125 * .5);
  // su_ncqo_init(&new->chroma_precenter, params->chroma_fhint - .45);
  // su_iir_brickwall_lp_init(&new->chroma_bandpass_i, 10, .5);

  su_ncqo_init(&new->chroma_precenter, params->chroma_fhint);
  su_iir_brickwall_lp_init(&new->chroma_bandpass_i, 15, .1);

  su_tv_comb_filter_init(&new->chroma_comb, 2 * params->line_len, SU_FALSE);

  //su_iir_bwlpf_init(&new->chroma_bandpass_i, 5, .5);
  
  
  //su_iir_brickwall_bp_init(&new->chroma_bandpass_i, 10, 0.5, params->chroma_fhint);
  //u_iir_brickwall_bp_skew_init(&new->chroma_bandpass_q, 10, 0.5, params->chroma_fhint);

  //su_iir_bwbpf_init(&new->chroma_bandpass, 4, .85 * params->chroma_fhint, params->chroma_fhint * 1.05);

  su_tv_processor_swap_field(new);
  su_tv_processor_swap_field(new);
  new->field_y = 209;

  return new;

fail:
  if (new != NULL)
    su_tv_processor_destroy(new);

  return NULL;
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
  SUCOMPLEX *line_buffer = NULL;
  SUCOMPLEX *tmp = NULL;

  SUSCOUNT delay_line_len = SU_CEIL(params->line_len);
  SUBOOL ok = SU_FALSE;

  SU_TRY_FAIL(params->line_len >= 1);
  SU_TRY_FAIL(params->frame_lines >= 1);

  SU_TRY_FAIL(!params->enable_sync || params->hsync_len >= 1);
  SU_TRY_FAIL(!params->enable_sync || params->vsync_len >= 1);

  /* Reset comb filter */
  self->delay_line_len = delay_line_len;

  SU_CONSTRUCT_FAIL(
    su_tv_pulse_separator,
    &self->sync_sep,
    params->hsync_len,
    params->line_len,
    0.2);

  if (params->enable_comb) {
    if (self->delay_line_len != delay_line_len || line_buffer == NULL) {
      SU_TRY_FAIL(tmp = realloc(line_buffer, sizeof(SUCOMPLEX) * delay_line_len));

      line_buffer = tmp;

      if (self->delay_line == NULL) {
        memset(line_buffer, 0, sizeof(SUCOMPLEX) * delay_line_len);
      } else if (delay_line_len > self->delay_line_len) {
        memset(
            line_buffer + self->delay_line_len,
            0,
            sizeof(SUCOMPLEX) * (delay_line_len - self->delay_line_len));
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
  self->chroma_alpha = SU_SPLPF_ALPHA(params->chroma_res_tau);

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

#define FEEDBACK_FORM

SUINLINE
SU_METHOD(su_tv_processor, void, comb_filter_feed, SUCOMPLEX x)
{
  SUCOMPLEX prev_x, y;
  int prev_ndx;

  if (self->delay_line != NULL) {
    if (self->delay_line_ptr >= self->delay_line_len)
      self->delay_line_ptr %= self->delay_line_len;

    prev_ndx = self->delay_line_len + self->delay_line_ptr - self->est_line_len;

    if (prev_ndx < 0)
      prev_ndx += self->delay_line_ptr;
    else
      prev_ndx %= self->delay_line_len;

    prev_x = self->delay_line[prev_ndx];

    if (self->params.comb_reverse)
      prev_x = -prev_x;

    self->delay_line[self->delay_line_ptr] = x;
    ++self->delay_line_ptr;

    y = .5 * (x + prev_x);

    self->delay_out_positive = y;
    self->delay_out_negative = x - y;
  } else {
    self->delay_out_positive = x;
    self->delay_out_negative = x;
  }
}

SUINLINE
SU_METHOD(su_tv_processor, SUCOMPLEX, pulse_filter_feed, SUCOMPLEX x)
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
    self->agc_gain += (self->agc_lines / self->agc_accum- self->agc_gain);
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
SU_METHOD(su_tv_processor, void, do_hsync)
{
  SUFLOAT xf = su_tv_processor_get_field_x(self);
  SUFLOAT line_len = self->sync_sep.line_len;
  SUFLOAT xf_offset =
      self->params.hsync_len / 2 + self->params.x_off * line_len;
  SUFLOAT xf_error = xf_offset - xf;


  /* 2. Horizontal offset ajustment. */
  if (SU_ABS(xf_error / line_len) > self->params.hsync_max_err) {
    self->state = SU_TV_PROCESSOR_SEARCH;

    if (SU_ABS(xf_error / line_len) > self->params.hsync_huge_err)
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

    if (SU_ABS(xf_error / line_len) < self->params.hsync_min_err) {
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

  return SU_TRUE;

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
SU_METHOD(su_tv_processor, SUBOOL, sync_feed, SUFLOAT pulse_x)
{
  #if 0
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

  #endif
}

SUINLINE
SU_METHOD(su_tv_processor, void, compute_chroma)
{
  SUSCOUNT line = su_tv_processor_get_line(self);
  SUSDIFF p    = line * self->delay_line_len;
  SUSCOUNT i, cstart, cend;
  SUCOMPLEX acc = 0, z, sym, carr;
  SUCOMPLEX chroma_dc = 0;
  SUCOMPLEX phase, w;
  SUFLOAT ang;
  SUCOMPLEX *curr_chroma, *prev_chroma;
  SUFLOAT   chroma_amp = 0;
  SUBOOL    odd_line = (line / 2 & 1) ^ (line & 1);
  su_tv_frame_buffer_t *curr_buffer = self->current;
  z = SU_C_EXP(I * (20) / 180. * M_PI);
  sym = 1;

  self->line_parity = !self->line_parity;
  self->line_count = (self->line_count + 1) & 3;

  curr_chroma = curr_buffer->chroma + p;

  if (line < 2)
    return;

  su_ncqo_set_angfreq(&self->chroma_precenter, self->chroma_pll.ncqo.omega);
  if (self->line_parity) {
    unsigned int i;

    prev_chroma = curr_chroma - 2 * self->delay_line_len;

    if (self->field_y == 125) {
      printf("Frame %d\n", self->frame_count);
      char name[100];
      sprintf(name, "chroma-even-%04d.csv", self->frame_count);
      FILE *fp = fopen(name, "wb");

      if (fp != NULL) {
        unsigned int i;

        for (i = 0; i < self->delay_line_len; ++i)
          fprintf(fp, "%+.10e,%+.10e\n",
          SU_C_ARG(curr_chroma[i]) / M_PI * 180,
          SU_C_ARG(prev_chroma[i]) / M_PI * 180);
        fclose(fp);
      }
    }

#if 1

    p = self->chroma_bandpass_i.x_size / 2 + 2;
    for (i = 0; p < self->delay_line_len; ++i) {
      curr_chroma[i] = SU_C_CONJ(curr_chroma[p++] * z) * sym;
      //curr_chroma[i] = 0;
      //prev_chroma[i] = curr_chroma[i];
    }
  } else {
    p = self->chroma_bandpass_i.x_size / 2 + 2;
    for (i = 0; p < self->delay_line_len; ++i) {
      curr_chroma[i] = curr_chroma[p++] * z * sym;
      //curr_chroma[i] = 0;
    }
#endif
  }

#if 0
  for (i = 0; i < self->delay_line_len; ++i) {
      curr_chroma[i] *= z;
      //curr_chroma[i] = 0;
  }
#endif

#if 0
  //su_ncqo_init(&ncqo, self->params.chroma_fhint);
  su_pll_init(&pll, self->params.chroma_fhint, 0.0003125 * 32);

  su_ncqo_set_phase(&pll.ncqo, M_PI / 2);

  /* Phase 1: determine chroma DC */
  cstart = -5 + -.35 * self->params.hsync_len + self->params.chroma_burst_start;
  cend   = -5 + -.35 * self->params.hsync_len +self->params.chroma_burst_start + self->params.chroma_burst_len;

  for (i = cstart; i < cend; ++i)
    chroma_dc += curr_buffer->chroma[p + i];
  chroma_dc /= (cend - cstart);

  /* Phase 2: determine chroma amplitude */
  for (i = cstart; i < cend; ++i) {
    z = curr_buffer->chroma[p + i] - chroma_dc;
    chroma_amp += z * SU_C_CONJ(z);
    //curr_buffer->buffer[p + i] = 1;
  }

  chroma_amp /= (cend - cstart);
  chroma_amp  = .5 * SU_SQRT(chroma_amp);

  //printf("Line %d: Chroma AMP: %g, is odd line? %d\n", line, chroma_amp, odd_line);
  /* Phase 3: train burst */
  w = 1;
  for (i = cstart; i < cend; ++i) {
    z = (curr_buffer->chroma[p + i] - chroma_dc) / chroma_amp;
    printf("INSTFREQ: %g\n", SU_C_ARG(z * SU_C_CONJ(w)) / M_PI * 180);
    w = z;
    z = su_pll_track(&pll, z);
    curr_buffer->buffer[p + i] = 1;
    curr_buffer->chroma[p + i] = z / SU_C_ABS(z); //su_pll_track(&pll, z * SU_C_CONJ(carr));
  }

  printf("(%g) - %g\n", pll.ncqo.omega / M_PI * 180, SU_C_ARG(z) / M_PI * 180);

  phase = SU_C_EXP(-I * SU_C_ARG(z));

  curr_chroma = curr_buffer->chroma + p;
  prev_chroma = curr_buffer->chroma + p - 2 * self->delay_line_len;

  /* Phase 4: decode */
  for (i = cend; i < self->delay_line_len; ++i) {
    carr = su_ncqo_read(&pll.ncqo);
    z = .1 * (curr_buffer->chroma[p + i] - chroma_dc) / chroma_amp;

    acc = phase * z * SU_C_CONJ(carr);

    curr_buffer->chroma[p + i] = acc;
    curr_buffer->buffer[p + i] = SU_C_ABS(acc);
/*
    if (odd_line)
      curr_chroma[i] = -acc;
    /*
      prev_chroma[i] = prev_chroma[i] - curr_chroma[i];
      curr_chroma[i] = prev_chroma[i];
    }*/
  }

  su_pll_finalize(&pll);
  #endif
}

SUINLINE
SU_METHOD(su_tv_processor, SUBOOL, frame_feed, SUFLOAT x, SUCOMPLEX iq)
{
  SUSCOUNT line;
  SUSCOUNT p;
  SUFLOAT xf;
  SUFLOAT beta = self->field_x_dec;
  SUFLOAT value =
      self->params.reverse ? self->agc_gain * x : (1 - self->agc_gain * x);
  su_tv_frame_buffer_t *curr_buffer = self->current;
  SUBOOL have_line = SU_FALSE;
  
  line = su_tv_processor_get_line(self);

  assert(!isnan(SU_C_REAL(iq)) && !isnan(SU_C_IMAG(iq)));

  value -= .3;
  value /= .7;

  if (self->field_x < self->delay_line_len && line < self->params.frame_lines) {
    p = line * self->delay_line_len + self->field_x;

    if (self->field_prev_ptr < p) {
      curr_buffer->buffer[self->field_prev_ptr] += (1 - beta) * value;
      curr_buffer->chroma[self->field_prev_ptr] += (1 - beta) * iq;
    }

    curr_buffer->buffer[p] = beta * value;
    curr_buffer->chroma[p] = beta * iq;

    self->field_prev_ptr = p;
  }

  ++self->field_x;

  xf = su_tv_processor_get_field_x(self);

  if (xf > self->sync_sep.line_len_orig) {
    if (xf < 2 * self->sync_sep.line_len_orig)
      su_tv_processor_set_field_x(self, xf - self->sync_sep.line_len_orig);
    else
      su_tv_processor_set_field_x(
          self,
          xf - self->sync_sep.line_len_orig * SU_FLOOR(xf / self->sync_sep.line_len_orig));

    have_line = SU_TRUE;

    if (self->params.enable_color)
      su_tv_processor_compute_chroma(self);

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

SUINLINE
SU_METHOD(su_tv_processor, void, feed_hsync, SUCOMPLEX x)
{
  if (su_tv_pulse_separator_feed(&self->sync_sep, x))
    su_tv_processor_do_hsync(self);
}

SU_METHOD(su_tv_processor, SUBOOL, feed, SUCOMPLEX x)
{
  SUBOOL have_frame = SU_FALSE;
  SUFLOAT pulse_x, amp;
  SUFLOAT gain_amp = SU_C_ABS(x);
  SUBOOL chroma_sync;
  SUCOMPLEX z;
  SU_TRYCATCH(su_tv_processor_assert_current_frame(self), return SU_FALSE);
  
  su_tv_processor_feed_hsync(self, x);

  if (self->chromafp == NULL) {
    char name[100];
    sprintf(name, "raw-chroma-%04d.raw", self->frame_count);

    self->chromafp = fopen(name, "wb");
  }
  /* TODO: Reuse amplitude here? */
  //pulse_x = su_tv_processor_pulse_filter_feed(self, gain_amp);

  //su_tv_processor_comb_filter_feed(self, gain_amp);

  /* Positive output: luminance (only care about amplitude)*/
  /* Negative output: chrominance (need IQ) */

  //amp = SU_C_ABS(self->delay_out_positive);
  //x = self->delay_out_negative;

  SU_SPLPF_FEED(self->delay_out_positive, gain_amp, SU_SPLPF_ALPHA(2));
  amp = self->delay_out_positive;

  SUSCOUNT diff;
  diff = self->field_x + self->sync_sep.sync_len + .5 * self->chroma_bandpass_i.x_size;

  chroma_sync = diff >= self->params.chroma_burst_start 
      && diff < (self->params.chroma_burst_start + self->params.chroma_burst_len);

  z = su_ncqo_read(&self->chroma_precenter);

  x = z * su_iir_filt_feed(&self->chroma_bandpass_i, x * SU_C_CONJ(z));

  if (chroma_sync && !self->chroma_sync) {
    self->chroma_sync = SU_TRUE;
    self->chroma_acc = 0;
  }

  if (chroma_sync) {
    self->chroma_acc += SU_C_ABS(x);
    x = su_pll_track(&self->chroma_pll, x * self->chroma_gain);
  } else {
    x *= SU_C_CONJ(su_ncqo_read(&self->chroma_pll.ncqo)) * self->chroma_gain * .4;
  }

  x = su_tv_comb_filter_feed(&self->chroma_comb, x);

  if (!chroma_sync && self->chroma_sync) {
    self->chroma_sync = SU_FALSE;
    self->chroma_acc /= self->params.chroma_burst_len;

    if (self->chroma_acc)
      SU_SPLPF_FEED(self->chroma_gain, 1 / SU_ABS(self->chroma_acc), self->chroma_alpha);

    self->chroma_off = self->chroma_acc < 10;
  }

  /*if (self->chroma_pll.lock > 1)
    x = 0;*/

  /*if (!self->chroma_off)
    x *= SU_C_EXP(I * 170. / 180. * M_PI);
  else
    x = 0;*/

  /*if (!self->line_parity & 1)
    x = SU_C_CONJ(x);*/
  // x *= SU_C_EXP(I * 170. / 180. * M_PI);
  if (self->params.enable_agc)
    su_tv_processor_line_agc_feed(self, amp);

  /*if (self->params.enable_sync)
    su_tv_processor_sync_feed(self, gain_amp);*/

  fwrite(&x, sizeof(SUCOMPLEX), 1, self->chromafp);

  if (su_tv_processor_frame_feed(self, amp, x)) {
    if (self->params.enable_agc)
      su_tv_processor_line_agc_commit(self);

    if (self->field_complete) {
      have_frame = !self->params.interlace
                   || self->field_parity == !!self->params.dominance;
      if (have_frame) {
        if (self->chromafp != NULL)
          fclose(self->chromafp);
        self->chromafp = NULL;
        self->last_frame = self->ptr;
        self->frame_count++;
      }

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
