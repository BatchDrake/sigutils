/*

  Copyright (C) 2021 Gonzalo José Carracedo Carballal

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

#define _SU_LOG_DOMAIN "apt"

#include <sigutils/specific/apt.h>

#include <sigutils/sigutils.h>
#include <sigutils/taps.h>
#include <stdio.h>
#include <string.h>

SUPRIVATE unsigned int
su_apt_decoder_correlate(
    su_apt_decoder_t *self,
    const SUCOMPLEX *pat,
    unsigned int start,
    unsigned int end,
    SUFLOAT *snr,
    SUFLOAT *delta)
{
  unsigned int i, where;
  unsigned int p = start;
  unsigned int len;
  int p_signed;
  SUFLOAT max, psd;
  SUFLOAT sig_noise = SU_APT_MIN_LEVEL;

  SUFLOAT mean, sum;

  /* Correlate */
  SU_FFTW(_execute)(self->direct_plan);
  for (i = 0; i < SU_APT_BUFF_LEN; ++i)
    self->corr_fft[i] *= SU_C_CONJ(pat[i]);
  SU_FFTW(_execute)(self->reverse_plan);

  /* Find pulse. This is actually a first guess. */
  max = SU_APT_MIN_LEVEL;
  where = start;
  len = (SU_APT_BUFF_LEN + end - start) % SU_APT_BUFF_LEN + 1;

  for (i = 0; i < len; ++i) {
    self->corr_fft[p] *= SU_C_CONJ(self->corr_fft[p]);
    psd = SU_C_REAL(self->corr_fft[p]);

    sig_noise += psd;
    if (psd > max) {
      where = p;
      max = psd;
    }

    if (++p >= SU_APT_BUFF_LEN)
      p = 0;
  }

  *snr = len * max / (sig_noise - max);

  /* Get the pulse position _with decimals_ */
  mean = sum = 0;
  for (i = 0; i < 7; ++i) {
    p_signed = where + i - 3;
    psd = SU_C_REAL(
        self->corr_fft[(SU_APT_BUFF_LEN + p_signed) % SU_APT_BUFF_LEN]);
    mean += p_signed * psd;
    sum += psd;
  }
  mean /= sum;

  *delta = mean - SU_FLOOR(mean);
  where = (SU_APT_BUFF_LEN + (int)SU_FLOOR(mean)) % SU_APT_BUFF_LEN;

  return where;
}

SUPRIVATE void
su_apt_decoder_update_levels(su_apt_decoder_t *self, SUBOOL detected)
{
  unsigned int i;

  SUFLOAT mean_white = 0;
  SUFLOAT mean_black = 0;

  SUFLOAT min = INFINITY;
  SUFLOAT max = -INFINITY;

  if (detected) {
    /* Find black and white levels */
    for (i = 0; i < SU_APT_LEVEL_LEN; ++i) {
      mean_black +=
          self->line_buffer[(SU_APT_BLACK_START + i) % SU_APT_LINE_BUFF_LEN];
      mean_white +=
          self->line_buffer[(SU_APT_WHITE_START + i) % SU_APT_LINE_BUFF_LEN];
    }

    // fprintf(stderr, "%d: %g\n", self->lines, mean_white - mean_black,
    // mean_white, mean_black);
    mean_black /= SU_APT_LEVEL_LEN;
    mean_white /= SU_APT_LEVEL_LEN;

    if (mean_black < mean_white
        && !sufeq(mean_white, mean_black, .5 * (mean_white + mean_black))) {
      if (!self->have_levels) {
        self->mean_black = mean_black;
        self->mean_white = mean_white;
        self->have_levels = SU_TRUE;
      } else {
        SU_SPLPF_FEED(self->mean_black, mean_black, self->line_len_alpha);
        SU_SPLPF_FEED(self->mean_white, mean_white, self->line_len_alpha);
      }
    }
  }

  /* No valid levels yet. Temporary guess based on signal power. */
  if (!self->have_levels) {
    for (i = 0; i < self->line_ptr; ++i) {
      if (min > self->line_buffer[i])
        min = self->line_buffer[i];
      if (max < self->line_buffer[i])
        max = self->line_buffer[i];
    }

    self->mean_black = min;
    self->mean_white = max + SU_APT_MIN_LEVEL;
  }
}

SUPRIVATE SUBOOL
su_apt_decoder_flush_line(su_apt_decoder_t *self, SUBOOL detected)
{
  uint8_t *line = NULL;
  unsigned int i, j, ndx;
  SUFLOAT beta, value;
  SUFLOAT range;
  int px;

  SUBOOL ok = SU_FALSE;

  if (self->line_ptr < SU_APT_CHANNEL_LEN)
    return SU_TRUE;

  SU_TRYCATCH(line = malloc(sizeof(uint8_t) * SU_APT_LINE_LEN), goto done);

  su_apt_decoder_update_levels(self, detected);
  range = self->mean_white - self->mean_black;

  for (i = 0; i < SU_APT_LINE_LEN; ++i) {
    ndx = (self->line_ptr - 1) * i / (SUFLOAT)(SU_APT_LINE_LEN - 1);
    j = SU_FLOOR(ndx);
    beta = ndx - j;

    if (j < self->line_ptr - 1)
      value =
          (1 - beta) * self->line_buffer[j] + beta * self->line_buffer[j + 1];
    else
      value = self->line_buffer[j];

    px = (unsigned int)(255 * (value - self->mean_black) / range);

    if (px > 255)
      px = 255;
    if (px < 0)
      px = 0;

    line[i] = px;
  }

  if (self->callbacks.on_line_data != NULL) {
    SU_TRYCATCH(
        (self->callbacks.on_line_data)(
            self,
            self->callbacks.userdata,
            self->lines,
            SU_APT_DECODER_CHANNEL_B,
            detected,
            line,
            SU_APT_CHANNEL_LEN),
        goto done);
    SU_TRYCATCH(
        (self->callbacks.on_line_data)(
            self,
            self->callbacks.userdata,
            self->lines + 1,
            SU_APT_DECODER_CHANNEL_A,
            detected,
            line + SU_APT_CHANNEL_LEN,
            SU_APT_CHANNEL_LEN),
        goto done);
  }

  SU_TRYCATCH(PTR_LIST_APPEND_CHECK(self->scan_line, line) != -1, goto done);
  line = NULL;

  memset(self->line_buffer, 0, self->line_ptr * sizeof(SUFLOAT));
  self->line_ptr = 0;

  ++self->lines;

  ok = SU_TRUE;

done:
  if (line != NULL)
    free(line);

  return ok;
}

SUPRIVATE void
su_apt_decoder_extract_line_until(su_apt_decoder_t *self, unsigned int pos)
{
  unsigned int p = self->line_last_samp;

  while (p != pos) {
    if (p >= SU_APT_LINE_BUFF_LEN)
      p = 0;

    if (self->line_ptr >= SU_APT_LINE_LEN)
      break;

    self->line_buffer[self->line_ptr++] =
        (1 - self->last_sync_delta) * self->samp_buffer[p]
        + (self->last_sync_delta)
              * self->samp_buffer[(p + 1) % SU_APT_LINE_BUFF_LEN];
    ++p;
  }

  self->line_last_samp = pos;
}

SUINLINE SUSCOUNT
su_apt_decoder_pos_to_abs(const su_apt_decoder_t *self, SUSCOUNT pos)
{
  return pos >= self->samp_ptr ? self->samp_epoch - SU_APT_BUFF_LEN + pos
                               : self->samp_epoch + pos;
}

SUPRIVATE SUBOOL
su_apt_decoder_perform_search(su_apt_decoder_t *self)
{
  unsigned int pos;
  SUSCOUNT abs_pos;
  SUFLOAT snr;
  SUSCOUNT line_len;
  SUSCOUNT next_search;
  unsigned int expected_sync;
  unsigned int search_start, search_end;
  SUBOOL have_line = SU_FALSE;
  SUFLOAT delta;
  SUBOOL ok = SU_FALSE;

  /*
   * If the last sync is still in the circular buffer, we intentionally
   * skip it from search.
   */
  if (self->count - self->last_sync < SU_APT_BUFF_LEN)
    search_start = (self->last_sync + SU_APT_SYNC_SIZE) % SU_APT_BUFF_LEN;
  else
    search_start = self->samp_ptr;
  search_end = (SU_APT_BUFF_LEN + self->samp_ptr - 1) % SU_APT_BUFF_LEN;

  next_search = self->count + self->line_len / 2;

  /* Correlate against SYNC B */
  pos = su_apt_decoder_correlate(
      self,
      self->sync_fft,
      search_start,
      search_end,
      &snr,
      &delta);
  abs_pos = su_apt_decoder_pos_to_abs(self, pos);

  if (snr > SU_APT_SYNC_MIN_SNR) {
    /*
     * Sync found!
     *
     * If the distance between this sync and the last one is compatible
     * with the line size, we assume it is a full line and adjust the true
     * line size accordingly.
     */

    if (self->callbacks.on_sync != NULL) {
      SU_TRYCATCH(
          (self->callbacks.on_sync)(self, self->callbacks.userdata, abs_pos),
          goto done);
    }

    line_len = abs_pos - self->last_sync;

    if (sufeq(line_len, SU_APT_LINE_LEN, 5e-2 * SU_APT_LINE_LEN)) {
      SU_SPLPF_FEED(self->line_len, line_len, self->line_len_alpha);

      if (self->callbacks.on_line != NULL) {
        SU_TRYCATCH(
            (self->callbacks.on_line)(
                self,
                self->callbacks.userdata,
                self->line_len),
            goto done);
      }

      have_line = SU_TRUE;
    }

    /* In any case, we already have a guess for the next sync */
    next_search = abs_pos + self->line_len + SU_APT_SYNC_SIZE;
    self->last_sync = abs_pos;
    self->next_sync = self->last_sync + self->line_len;

    su_apt_decoder_extract_line_until(self, pos);
    su_apt_decoder_flush_line(self, have_line);

    self->last_sync_delta = delta;
  } else {
    if (self->lines > 0) {
      if (su_apt_decoder_pos_to_abs(self, search_start) < self->next_sync
          && self->next_sync + SU_APT_SYNC_SIZE
                 < su_apt_decoder_pos_to_abs(self, search_end)) {
        next_search = self->next_sync + self->line_len + SU_APT_SYNC_SIZE;
        expected_sync = self->next_sync % SU_APT_LINE_BUFF_LEN;
        self->next_sync += SU_FLOOR(self->line_len);
        delta = 0;

        su_apt_decoder_extract_line_until(self, expected_sync);
        su_apt_decoder_flush_line(self, SU_FALSE);

        self->last_sync_delta = delta;
      }
    }
  }

  su_apt_decoder_extract_line_until(self, self->samp_ptr);
  self->next_search = next_search;

  ok = SU_TRUE;

done:
  return ok;
}

void
su_apt_decoder_set_snr(su_apt_decoder_t *self, SUFLOAT snr)
{
  self->sync_snr = snr;
}

SUBOOL
su_apt_decoder_feed_ex(
    su_apt_decoder_t *self,
    SUBOOL phase_only,
    const SUCOMPLEX *buffer,
    SUSCOUNT count)
{
  SUSCOUNT i;
  SUCOMPLEX x;
  SUFLOAT pwr;
  SUFLOAT snr;

  for (i = 0; i < count; ++i) {
    x = su_iir_filt_feed(
      &self->mf, 
      su_pll_track(
        &self->pll, 
        phase_only 
        ? SU_C_ARG(buffer[i])
        : buffer[i]));

    if (su_sampler_feed(&self->resampler, &x)) {
      pwr = SU_C_REAL(x * SU_C_CONJ(x));
      self->mean_i += SU_C_REAL(x) * SU_C_REAL(x);
      self->mean_q += SU_C_IMAG(x) * SU_C_IMAG(x);

      self->samp_buffer[self->samp_ptr++] = pwr;
      if (self->samp_ptr >= SU_APT_BUFF_LEN) {
        snr = SU_ABS(SU_POWER_DB_RAW(self->mean_i / self->mean_q));

        if (self->callbacks.on_carrier != NULL)
          SU_TRYCATCH(
              (self->callbacks.on_carrier)(
                  self,
                  self->callbacks.userdata,
                  SU_POWER_DB(snr)),
              return SU_FALSE);

        self->samp_ptr = 0;
        self->mean_i = self->mean_q = 0;
        self->samp_epoch += SU_APT_BUFF_LEN;
      }

      ++self->count;

      if (self->count >= self->next_search) {
        SU_TRYCATCH(su_apt_decoder_perform_search(self), return SU_FALSE);
      }
    }
  }

  return SU_TRUE;
}

SUBOOL
su_apt_decoder_feed(
    su_apt_decoder_t *self,
    const SUCOMPLEX *buffer,
    SUSCOUNT count)
{
  return su_apt_decoder_feed_ex(self, SU_FALSE, buffer, count);
}

void
su_apt_decoder_clear_image(su_apt_decoder_t *self)
{
  unsigned int i;

  for (i = 0; i < self->scan_line_count; ++i)
    free(self->scan_line_list[i]);

  if (self->scan_line_list != NULL)
    free(self->scan_line_list);

  self->scan_line_count = 0;
  self->scan_line_list = NULL;
}

SUBOOL
su_apt_decoder_dump_pgm(const su_apt_decoder_t *self, const char *path)
{
  FILE *fp = NULL;
  unsigned int i, j;
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(fp = fopen(path, "w"), goto fail);

  fprintf(fp, "P2\n");
  fprintf(fp, "# Generated by BatchDrake's APT Hack\n");
  fprintf(fp, "%d %u\n", SU_APT_LINE_LEN, self->scan_line_count);
  fprintf(fp, "255\n");

  for (j = 1; j < self->scan_line_count; ++j) {
    for (i = 0; i < SU_APT_CHANNEL_LEN; ++i)
      fprintf(fp, "%4d", self->scan_line_list[j][i + SU_APT_CHANNEL_LEN]);
    for (i = 0; i < SU_APT_CHANNEL_LEN; ++i)
      fprintf(fp, "%4d", self->scan_line_list[j - 1][i]);

    fprintf(fp, "\n");
  }

  ok = SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return ok;
}

/*
 * fs: samples per second
 * SU_APT_IF_RATE: words per second
 * fs / SU_APT_IF_RATE: samples per word
 */

su_apt_decoder_t *
su_apt_decoder_new(SUFLOAT fs, const struct sigutils_apt_decoder_callbacks *cb)
{
  su_apt_decoder_t *new = NULL;
  SU_FFTW(_plan) pattern_plan = NULL;
  unsigned int i = 0, j;
  SUFLOAT kinv;
  SUFLOAT bw;
  SUFLOAT samps_per_word = fs / SU_APT_IF_RATE;
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(new = calloc(1, sizeof(su_apt_decoder_t)), goto done);

  if (cb != NULL)
    new->callbacks = *cb;

  new->samp_rate = fs;
  new->line_len = SU_APT_LINE_LEN;
  new->line_len_alpha = SU_SPLPF_ALPHA(SU_APT_TRAINING_LINES);
  new->sync_snr = SU_APT_SYNC_MIN_SNR;
  new->next_search = new->line_len / 2;

  /* Setup PLL */
  bw = SU_ABS2NORM_FREQ(fs, SU_APT_AM_BANDWIDTH);

  SU_TRYCATCH(
      pattern_plan = su_lib_plan_dft_1d(
          SU_APT_BUFF_LEN,
          new->sync_fft,
          new->sync_fft,
          FFTW_FORWARD,
          su_lib_fftw_strategy()),
      goto done);

  SU_TRYCATCH(
      new->direct_plan = su_lib_plan_dft_1d(
          SU_APT_BUFF_LEN,
          new->samp_buffer,
          new->corr_fft,
          FFTW_FORWARD,
          su_lib_fftw_strategy()),
      goto done);

  SU_TRYCATCH(
      new->reverse_plan = su_lib_plan_dft_1d(
          SU_APT_BUFF_LEN,
          new->corr_fft,
          new->corr_fft,
          FFTW_BACKWARD,
          su_lib_fftw_strategy()),
      goto done);

  su_pll_init(
      &new->pll,
      SU_ABS2NORM_FREQ(fs, SU_APT_AM_CARRIER_FREQ),
      .001f * bw);

  /* Setup matched filter for 4160 Hz */
  SU_TRYCATCH(
      su_iir_rrc_init(
          &new->mf,
          5 * SU_CEIL(2 * samps_per_word),
          2 * samps_per_word,
          .55),
      goto done);

  /* Set resampler */
  SU_TRYCATCH(
      su_sampler_init(&new->resampler, SU_ABS2NORM_BAUD(fs, SU_APT_IF_RATE)),
      goto done);

  kinv = 1. / SU_APT_SYNC_SIZE;

  /* Configure channel B detector */
  for (i = 0; i < SU_APT_SYNC_SIZE; ++i) {
    j = (i - 4) % 5;
    new->sync_fft[i] = (i >= 4 && j < 3) ? kinv : -kinv;
  }

  SU_FFTW(_execute)(pattern_plan);

  ok = SU_TRUE;

done:
  if (!ok) {
    if (new != NULL) {
      su_apt_decoder_destroy(new);
      new = NULL;
    }
  }

  if (pattern_plan != NULL)
    SU_FFTW(_destroy_plan)(pattern_plan);

  return new;
}

void
su_apt_decoder_destroy(su_apt_decoder_t *self)
{
  if (self->reverse_plan != NULL)
    SU_FFTW(_destroy_plan)(self->reverse_plan);

  if (self->direct_plan != NULL)
    SU_FFTW(_destroy_plan)(self->direct_plan);

  su_apt_decoder_clear_image(self);

  su_sampler_finalize(&self->resampler);
  su_iir_filt_finalize(&self->mf);
  su_pll_finalize(&self->pll);

  free(self);
}
