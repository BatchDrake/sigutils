/*

  Copyright (C) 2018 Gonzalo José Carracedo Carballal

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

#define SU_LOG_DOMAIN "specttuner"

#include <string.h>
#include <stdlib.h>
#include "sampling.h"
#include "taps.h"
#include "specttuner.h"
#include "log.h"

SUPRIVATE void
su_specttuner_channel_destroy(su_specttuner_channel_t *channel)
{
  if (channel->plan[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_destroy_plan) (channel->plan[SU_SPECTTUNER_STATE_EVEN]);

  if (channel->plan[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_destroy_plan) (channel->plan[SU_SPECTTUNER_STATE_ODD]);

  if (channel->ifft[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_free) (channel->ifft[SU_SPECTTUNER_STATE_EVEN]);

  if (channel->ifft[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_free) (channel->ifft[SU_SPECTTUNER_STATE_ODD]);

  if (channel->fft != NULL)
    SU_FFTW(_free) (channel->fft);

  if (channel->window != NULL)
    SU_FFTW(_free) (channel->window);

  if (channel->h != NULL)
    SU_FFTW(_free) (channel->h);

  free(channel);
}

SUPRIVATE SUBOOL
su_specttuner_init_filter_response(
    const su_specttuner_t *owner,
    su_specttuner_channel_t *channel)
{
  SUBOOL ok = SU_FALSE;
  SU_FFTW(_plan) forward = NULL;
  SU_FFTW(_plan) backward = NULL;
  SUCOMPLEX tmp;
  unsigned int window_size = owner->params.window_size;
  unsigned int window_half = window_size / 2;
  unsigned int i;

  /* Backward plan */
  SU_TRYCATCH(
      forward = SU_FFTW(_plan_dft_1d)(
          window_size,
          channel->h,
          channel->h,
          FFTW_FORWARD,
          FFTW_ESTIMATE),
      goto done);

  /* Forward plan */
  SU_TRYCATCH(
      backward = SU_FFTW(_plan_dft_1d)(
          window_size,
          channel->h,
          channel->h,
          FFTW_BACKWARD,
          FFTW_ESTIMATE),
      goto done);

  /* First step: Setup ideal filter response */
  memset(channel->h, 0, sizeof(SUCOMPLEX) * window_size);

  for (i = 0; i < channel->halfw; ++i) {
    channel->h[i] = 1;
    channel->h[window_size - i - 1] = 1;
  }

  /* Second step: switch to time domain */
  SU_FFTW(_execute) (backward);

  /* Third step: recenter coefficients to apply window function */
  for (i = 0; i < window_half; ++i) {
    tmp = channel->h[i];
    channel->h[i] = channel->k * channel->h[window_half + i];
    channel->h[window_half + i] = channel->k * tmp;
  }

  /* Fourth step: apply Window function */
  su_taps_apply_blackmann_harris_complex(channel->h, window_size);

  /* Fifth step: recenter back */
  for (i = 0; i < window_half; ++i) {
    tmp = channel->h[i];
    channel->h[i] = channel->h[window_half + i];
    channel->h[window_half + i] = tmp;
  }

  /* Sixth step: move back to frequency domain */
  SU_FFTW(_execute) (forward);

  ok = SU_TRUE;

done:
  if (forward != NULL)
    SU_FFTW(_destroy_plan) (forward);

  if (backward != NULL)
    SU_FFTW(_destroy_plan) (backward);

  return ok;
}

void
su_specttuner_set_channel_freq(
    const su_specttuner_t *st,
    su_specttuner_channel_t *channel,
    SUFLOAT f0)
{
  unsigned int window_size = st->params.window_size;
  su_ncqo_t lo = su_ncqo_INITIALIZER;
  SUFLOAT off;

  channel->params.f0 = f0;
  channel->center = 2 * SU_ROUND(f0 / (4 * PI) * window_size);

  if (channel->params.precise) {
    off = f0 - channel->center * (2 * PI) / (SUFLOAT) window_size;
    off *= channel->decimation;
    su_ncqo_init_fixed(&channel->lo, SU_ANG2NORM_FREQ(off));
  }
}

SUPRIVATE su_specttuner_channel_t *
su_specttuner_channel_new(
    const su_specttuner_t *owner,
    const struct sigutils_specttuner_channel_params *params)
{
  su_specttuner_channel_t *new = NULL;
  unsigned int window_size = owner->params.window_size;
  unsigned int n = 1;
  unsigned int i;
  unsigned int min_size;
  SUFLOAT actual_bw;
  SUFLOAT off;

  SU_TRYCATCH(params->guard >= 1, goto fail);
  SU_TRYCATCH(params->bw > 0  && params->bw < 2 * PI, goto fail);
  SU_TRYCATCH(params->f0 >= 0 && params->f0 < 2 * PI, goto fail);

  SU_TRYCATCH(new = calloc(1, sizeof(su_specttuner_channel_t)), goto fail);

  actual_bw = params->bw * params->guard;

  SU_TRYCATCH(actual_bw > 0 && actual_bw < 2 * PI, goto fail);

  new->params = *params;
  new->index = -1;

  /* Tentative configuration */
  new->k = 1. / (2 * PI / actual_bw);

  /*
   * XXX: THERE IS SOMETHING HERE I COULD NOT FULLY UNDERSTAND
   *
   * For some reason, if we do not pick an even FFT bin for frequency
   * centering, AM components will show up at the decimator output. This
   * is probably related to some symmetry condition not being met
   *
   * TODO: Look into this ASAP
   */
  new->center = 2 * SU_ROUND(params->f0 / (4 * PI) * window_size);
  min_size    = SU_CEIL(new->k * window_size);

  /* Find the nearest power of 2 than can hold all these samples */
  while (n < min_size)
    n <<= 1;

  new->size = n;

  /* Adjust configuration to new size */
  new->decimation = window_size / new->size;
  new->k = 1. / (new->decimation * new->size);

  /*
   * High precision mode: initialize local oscilator to compensate
   * for rounding errors introduced by bin index calculation
   */
  if (params->precise) {
    off = params->f0 - new->center * (2 * PI) / (SUFLOAT) window_size;
    off *= new->decimation;
    su_ncqo_init_fixed(&new->lo, SU_ANG2NORM_FREQ(off));
  }

  new->halfsz = new->size >> 1;
  new->offset = new->size >> 2;

  new->width  = SU_CEIL(min_size / params->guard);
  new->halfw  = new->width >> 1;


  SU_TRYCATCH(new->width > 0, goto fail);

  /*
   * Window function. We leverage fftw(f)_malloc aligned addresses
   * to attempt some kind of cache efficiency here.
   */
  SU_TRYCATCH(
      new->window = SU_FFTW(_malloc)(new->size * sizeof(SUFLOAT)),
      goto fail);

  SU_TRYCATCH(
      new->h     = SU_FFTW(_malloc)(window_size * sizeof(SU_FFTW(_complex))),
      goto fail);

  SU_TRYCATCH(su_specttuner_init_filter_response(owner, new), goto fail);

  /*
   * Squared cosine window. Seems odd, right? Well, it turns out that
   * since we are storing the square of half a cycle, when we add the
   * odd and even halves we are actually adding up something weighted
   * by two squared cosine halves DELAYED one quarter of a cycle.
   *
   * This is equivalent to adding up something weighted by a squared SINE
   * and a squared COSINE. And it can be proven that cos^2 + sin^2 = 1.
   *
   * In the end, we are favouring central IFFT values before those in
   * the borders. This is something that we generally want.
   *
   * PS: We use SU_SIN instead of SU_COS because we are assuming that
   * the 0 is at new->size/2.
   */
  for (i = 0; i < new->size; ++i) {
    new->window[i] = SU_SIN(PI * (SUFLOAT) i / new->size);
    new->window[i] *= new->window[i];
  }

  /* FFT initialization */
  SU_TRYCATCH(
      new->ifft[SU_SPECTTUNER_STATE_EVEN] =
          SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))),
      goto fail);

  SU_TRYCATCH(
      new->ifft[SU_SPECTTUNER_STATE_ODD] =
          SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))),
      goto fail);

  SU_TRYCATCH(
      new->fft = SU_FFTW(_malloc)(new->size * sizeof(SU_FFTW(_complex))),
      goto fail);

  memset(new->fft, 0, new->size * sizeof(SU_FFTW(_complex)));

  memset(
      new->ifft[SU_SPECTTUNER_STATE_EVEN],
      0,
      new->size * sizeof(SU_FFTW(_complex)));

  memset(
      new->ifft[SU_SPECTTUNER_STATE_ODD],
      0,
      new->size * sizeof(SU_FFTW(_complex)));

  SU_TRYCATCH(
      new->plan[SU_SPECTTUNER_STATE_EVEN] =
          SU_FFTW(_plan_dft_1d)(
              new->size,
              new->fft,
              new->ifft[SU_SPECTTUNER_STATE_EVEN],
              FFTW_BACKWARD,
              FFTW_ESTIMATE),
          goto fail);

  SU_TRYCATCH(
      new->plan[SU_SPECTTUNER_STATE_ODD] =
          SU_FFTW(_plan_dft_1d)(
              new->size,
              new->fft,
              new->ifft[SU_SPECTTUNER_STATE_ODD],
              FFTW_BACKWARD,
              FFTW_ESTIMATE),
          goto fail);

  return new;

fail:
  if (new != NULL)
    su_specttuner_channel_destroy(new);

  return NULL;
}

void
su_specttuner_destroy(su_specttuner_t *st)
{
  unsigned int i;

  for (i = 0; i < st->channel_count; ++i)
    if (st->channel_list[i] != NULL)
      su_specttuner_close_channel(st, st->channel_list[i]);

  if (st->channel_list != NULL)
    free(st->channel_list);

  if (st->plans[SU_SPECTTUNER_STATE_EVEN] != NULL)
    SU_FFTW(_destroy_plan) (st->plans[SU_SPECTTUNER_STATE_EVEN]);

  if (st->plans[SU_SPECTTUNER_STATE_ODD] != NULL)
    SU_FFTW(_destroy_plan) (st->plans[SU_SPECTTUNER_STATE_ODD]);

  if (st->fft != NULL)
    SU_FFTW(_free) (st->fft);

  if (st->window != NULL)
    SU_FFTW(_free) (st->window);

  free(st);
}

su_specttuner_t *
su_specttuner_new(const struct sigutils_specttuner_params *params)
{
  su_specttuner_t *new = NULL;

  SU_TRYCATCH((params->window_size & 1) == 0, goto fail);

  SU_TRYCATCH(new = calloc(1, sizeof(su_specttuner_t)), goto fail);

  new->params = *params;
  new->half_size = params->window_size >> 1;
  new->full_size = 3 * params->window_size;

  /* Window is 3/2 the FFT size */
  SU_TRYCATCH(
      new->window = SU_FFTW(_malloc(
          new->full_size * sizeof(SU_FFTW(_complex)))),
      goto fail);

  /* FFT is the size provided by params */
  SU_TRYCATCH(
      new->fft = SU_FFTW(_malloc(
          params->window_size * sizeof(SU_FFTW(_complex)))),
      goto fail);

  /* Even plan starts at the beginning of the window */
  SU_TRYCATCH(
      new->plans[SU_SPECTTUNER_STATE_EVEN] = SU_FFTW(_plan_dft_1d)(
          params->window_size,
          new->window,
          new->fft,
          FFTW_FORWARD,
          FFTW_ESTIMATE),
      goto fail);

  /* Odd plan stars at window_size / 2 */
  SU_TRYCATCH(
      new->plans[SU_SPECTTUNER_STATE_ODD] = SU_FFTW(_plan_dft_1d)(
          params->window_size,
          new->window + new->half_size,
          new->fft,
          FFTW_FORWARD,
          FFTW_ESTIMATE),
      goto fail);

  return new;

fail:
  if (new != NULL)
    su_specttuner_destroy(new);

  return NULL;
}

SUINLINE SUSCOUNT
__su_specttuner_feed_bulk(
    su_specttuner_t *st,
    const SUCOMPLEX *buf,
    SUSCOUNT size)
{
  SUSDIFF halfsz;
  SUSDIFF p;

  if (size + st->p > st->params.window_size)
    size = st->params.window_size - st->p;

  switch (st->state)
  {
    case SU_SPECTTUNER_STATE_EVEN:
      /* Just copy at the beginning */
      memcpy(st->window + st->p, buf, size * sizeof(SUCOMPLEX));
      break;

    case SU_SPECTTUNER_STATE_ODD:
      /* Copy to the second third */
      memcpy(st->window + st->p + st->half_size, buf, size * sizeof(SUCOMPLEX));

      /* Did this copy populate the last third? */
      if (st->p + size > st->half_size) {
        halfsz = st->p + size - st->half_size;
        p = st->p > st->half_size ? st->p : st->half_size;

        /* Don't take into account data already written */
        halfsz -= p - st->half_size;

        /* Copy to the first third */
        if (halfsz > 0)
          memcpy(
              st->window + p - st->half_size,
              st->window + p + st->half_size,
              halfsz * sizeof(SUCOMPLEX));
      }
  }

  st->p += size;

  if (st->p == st->params.window_size) {
    st->p = st->half_size;

    /* Compute FFT */
    SU_FFTW(_execute) (st->plans[st->state]);

    /* Toggle state */
    st->state = !st->state;
    st->ready = SU_TRUE;
  }

  return size;
}

SUINLINE SUBOOL
__su_specttuner_feed_channel(
    const su_specttuner_t *st,
    su_specttuner_channel_t *channel)
{
  int p;
  int len;
  int window_size = st->params.window_size;
  unsigned int i;
  SUCOMPLEX phase;
  SUFLOAT alpha, beta;
  SUCOMPLEX *prev, *curr;

  p = channel->center;

  /***************************** Upper sideband ******************************/
  len = channel->halfw;
  if (p + len > window_size) /* Test for rollover */
    len = window_size - p;

  /* Copy to the end */
  memcpy(
      channel->fft,
      st->fft + p,
      len * sizeof(SUCOMPLEX));

  /* Copy remaining part */
  if (len < channel->halfw)
    memcpy(
        channel->fft + len,
        st->fft,
        (channel->halfw - len) * sizeof(SUCOMPLEX));

  /***************************** Lower sideband ******************************/
  len = channel->halfw;
  if (p < len) /* Roll over */
    len = p; /* Can copy up to p bytes */


  /* Copy higher frequencies */
  memcpy(
      channel->fft + channel->size - len,
      st->fft + p - len,
      len * sizeof(SUCOMPLEX));

  /* Copy remaining part */
  if (len < channel->halfw)
    memcpy(
        channel->fft + channel->size - channel->halfw,
        st->fft + window_size - (channel->halfw - len),
        (channel->halfw - len) * sizeof(SUCOMPLEX));

  /*********************** Apply filter and scaling **************************/
#ifdef SU_SPECTTUNER_SQUARE_FILTER
  for (i = 0; i < channel->halfw; ++i)
    channel->fft[i] *= channel->k;

  for (i = channel->size - channel->halfw; i < channel->size; ++i)
    channel->fft[i] *= channel->k;
#else
  for (i = 0; i < channel->halfsz; ++i) {
    channel->fft[i] *= channel->k * channel->h[i];
    channel->fft[channel->size - i - 1] *=
        channel->k * channel->h[window_size - i - 1];
  }
#endif
  /************************* Back to time domain******************************/
  SU_FFTW(_execute) (channel->plan[channel->state]);

  curr = channel->ifft[channel->state];
  prev = channel->ifft[!channel->state] + channel->halfsz;

  /* Glue buffers */
  if (channel->params.precise) {
    for (i = 0; i < channel->halfsz; ++i) {
      alpha = channel->window[i]; /* Positive slope */
      beta  = channel->window[i + channel->halfsz]; /* Negative slope */

      phase = su_ncqo_read(&channel->lo);

      curr[i] = phase * (alpha * curr[i] + beta * prev[i]);
    }
  } else {
    for (i = 0; i < channel->halfsz; ++i) {
      alpha = channel->window[i]; /* Positive slope */
      beta  = channel->window[i + channel->halfsz]; /* Negative slope */

      curr[i] = alpha * curr[i] + beta * prev[i];
    }
  }

  channel->state = !channel->state;

  /************************** Call user callback *****************************/
  return (channel->params.on_data) (
      channel,
      channel->params.privdata,
      curr,
      channel->halfsz);
}

SUSDIFF
su_specttuner_feed_bulk_single(
    su_specttuner_t *st,
    const SUCOMPLEX *buf,
    SUSCOUNT size)
{
  SUSDIFF got;
  SUSCOUNT ok = SU_TRUE;
  unsigned int i;

  if (st->ready)
    return 0;

  got = __su_specttuner_feed_bulk(st, buf, size);

  /* Buffer full, feed channels */
  if (st->ready)
    for (i = 0; i < st->channel_count; ++i)
      if (st->channel_list[i] != NULL)
        ok = __su_specttuner_feed_channel(st, st->channel_list[i]) && ok;

  return ok ? got : -1;
}

SUBOOL
su_specttuner_feed_bulk(
    su_specttuner_t *st,
    const SUCOMPLEX *buf,
    SUSCOUNT size)
{
  SUSDIFF got;
  SUBOOL ok = SU_TRUE;

  while (size > 0) {
    got = su_specttuner_feed_bulk_single(st, buf, size);

    if (su_specttuner_new_data(st))
      su_specttuner_ack_data(st);

    if (got == -1)
      ok = SU_FALSE;

    buf += got;
    size -= got;
  }

  return ok;
}

su_specttuner_channel_t *
su_specttuner_open_channel(
    su_specttuner_t *st,
    const struct sigutils_specttuner_channel_params *params)
{
  su_specttuner_channel_t *new = NULL;
  int index;

  SU_TRYCATCH(new = su_specttuner_channel_new(st, params), goto fail);

  SU_TRYCATCH(
      (index = PTR_LIST_APPEND_CHECK(st->channel, new)) != -1,
      goto fail);

  new->index = index;

  ++st->count;

  return new;

fail:
  if (new != NULL)
    su_specttuner_channel_destroy(new);

  return NULL;
}

SUBOOL
su_specttuner_close_channel(
    su_specttuner_t *st,
    su_specttuner_channel_t *channel)
{
  SU_TRYCATCH(channel->index >= 0, return SU_FALSE);

  SU_TRYCATCH(channel->index < st->channel_count, return SU_FALSE);

  SU_TRYCATCH(st->channel_list[channel->index] == channel, return SU_FALSE);

  st->channel_list[channel->index] = NULL;

  su_specttuner_channel_destroy(channel);

  --st->count;

  return SU_TRUE;
}
