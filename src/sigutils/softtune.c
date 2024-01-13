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

#include <assert.h>
#include <string.h>

#define SU_LOG_DOMAIN "softtuner"

#include <sigutils/sampling.h>
#include <sigutils/softtune.h>

void
su_softtuner_params_adjust_to_channel(
    struct sigutils_softtuner_params *params,
    const struct sigutils_channel *channel)
{
  SUFLOAT width;

  width = MAX(channel->f_hi - channel->f_lo, channel->bw);

  if ((params->decimation = .3 * SU_CEIL(params->samp_rate / width)) < 1)
    params->decimation = 1;

  params->bw = width;
  params->fc = channel->fc - channel->ft;
}

SUBOOL
su_softtuner_init(
    su_softtuner_t *tuner,
    const struct sigutils_softtuner_params *params)
{
  assert(params->samp_rate > 0);
  assert(params->decimation > 0);

  memset(tuner, 0, sizeof(su_softtuner_t));

  tuner->params = *params;
  tuner->avginv = 1. / params->decimation;

  SU_TRYCATCH(
      su_stream_init(&tuner->output, SU_BLOCK_STREAM_BUFFER_SIZE),
      goto fail);

  su_ncqo_init_fixed(
      &tuner->lo,
      SU_ABS2NORM_FREQ(params->samp_rate, params->fc));

  if (params->bw > 0.0) {
    SU_TRYCATCH(
        su_iir_bwlpf_init(
            &tuner->antialias,
            SU_SOFTTUNER_ANTIALIAS_ORDER,
            .5 * SU_ABS2NORM_FREQ(params->samp_rate, params->bw)
                * SU_SOFTTUNER_ANTIALIAS_EXTRA_BW),
        goto fail);
    tuner->filtered = SU_TRUE;
  }

  return SU_TRUE;

fail:
  su_softtuner_finalize(tuner);

  return SU_FALSE;
}

SUSCOUNT
su_softtuner_feed(su_softtuner_t *tuner, const SUCOMPLEX *input, SUSCOUNT size)
{
  SUSCOUNT i = 0;
  SUCOMPLEX x;
  SUSCOUNT avail;
  SUCOMPLEX *buf;
  SUSCOUNT n = 0;

  avail = su_stream_get_contiguous(
      &tuner->output,
      &buf,
      SU_BLOCK_STREAM_BUFFER_SIZE);

  SU_TRYCATCH(avail > 0, return 0);

  buf[0] = 0;

  for (i = 0; i < size && n < avail; ++i) {
    /* Carrier centering. Must happen *before* decimation */
    x = input[i] * SU_C_CONJ(su_ncqo_read(&tuner->lo));

    if (tuner->filtered)
      x = su_iir_filt_feed(&tuner->antialias, x);

    if (tuner->params.decimation > 1) {
      if (++tuner->decim_ptr < tuner->params.decimation) {
        buf[n] += tuner->avginv * x;
      } else {
        if (++n < avail)
          buf[n] = 0;
        tuner->decim_ptr = 0; /* Reset decimation pointer */
      }
    } else {
      buf[n++] = x;
    }
  }

  su_stream_advance_contiguous(&tuner->output, n);

  return i;
}

SUSDIFF
su_softtuner_read(su_softtuner_t *tuner, SUCOMPLEX *out, SUSCOUNT size)
{
  SUSDIFF result;

  result = su_stream_read(&tuner->output, tuner->read_ptr, out, size);

  if (result == -1) {
    SU_ERROR("Samples lost while reading from tuner!\n");
    tuner->read_ptr = su_stream_tell(&tuner->output);
    return 0;
  }

  tuner->read_ptr += result;

  return result;
}

void
su_softtuner_finalize(su_softtuner_t *tuner)
{
  if (tuner->filtered)
    su_iir_filt_finalize(&tuner->antialias);

  su_stream_finalize(&tuner->output);

  memset(tuner, 0, sizeof(su_softtuner_t));
}
