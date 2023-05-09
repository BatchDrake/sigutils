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

#ifndef _SIGUTILS_SPECIFIC_APT_H
#define _SIGUTILS_SPECIFIC_APT_H

#include <sigutils/clock.h>
#include <sigutils/detect.h>
#include <sigutils/pll.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* https://web.archive.org/web/20070505090431/http://www2.ncdc.noaa.gov/docs/klm/html/c4/sec4-2.htm
 */

#define SU_APT_IF_RATE 4160
#define SU_APT_LINE_LEN (SU_APT_IF_RATE / 2)
#define SU_APT_CHANNEL_LEN (SU_APT_LINE_LEN / 2)

#define SU_APT_SYNC_SIZE 39 /* By spec */
#define SU_APT_SYNC_MIN_SNR 40

#define SU_APT_AM_CARRIER_FREQ 2400
#define SU_APT_AM_BANDWIDTH 2400

#define SU_APT_BUFF_LEN (2 * SU_APT_LINE_LEN + 2 * SU_APT_SYNC_SIZE)
#define SU_APT_LINE_BUFF_LEN SU_APT_BUFF_LEN
#define SU_APT_TRAINING_LINES 50
#define SU_APT_LINE_LEN_THRESHOLD (SU_APT_SYNC_SIZE / 2)
#define SU_APT_MINUTE_MARKER_LEN 47
#define SU_APT_VIDEO_DATA_LEN 909
#define SU_APT_TELEMETRY_LEN 45
#define SU_APT_SYNC_B_OFFSET                                          \
  (SU_APT_SYNC_SIZE + SU_APT_MINUTE_MARKER_LEN + SU_APT_TELEMETRY_LEN \
   + SU_APT_VIDEO_DATA_LEN)
#define SU_APT_LEVEL_LEN 10
#define SU_APT_BLACK_START 1085
#define SU_APT_WHITE_START 45
#define SU_APT_MIN_CARRIER_DB 1
#define SU_APT_MIN_LEVEL 1e-30

enum sigutils_apt_decoder_channel {
  SU_APT_DECODER_CHANNEL_A,
  SU_APT_DECODER_CHANNEL_B
};

struct sigutils_apt_decoder;

struct sigutils_apt_decoder_callbacks {
  void *userdata;

  SUBOOL (*on_carrier)(struct sigutils_apt_decoder *, void *, SUFLOAT);
  SUBOOL (*on_sync)(struct sigutils_apt_decoder *, void *, SUSCOUNT);
  SUBOOL (*on_line)(struct sigutils_apt_decoder *, void *, SUFLOAT);
  SUBOOL(*on_line_data)
  (struct sigutils_apt_decoder *,
   void *,
   SUSCOUNT,
   enum sigutils_apt_decoder_channel,
   SUBOOL,
   const uint8_t *,
   size_t);
};

#define sigutils_apt_decoder_callbacks_INITIALIZER \
  {                                                \
    NULL,     /* userdata */                       \
        NULL, /* on_carrier */                     \
        NULL, /* on_sync */                        \
        NULL, /* on_line */                        \
        NULL, /* on_line_data */                   \
  }

struct sigutils_apt_decoder {
  SUFLOAT samp_rate;
  su_pll_t pll;           /* To center carrier */
  su_iir_filt_t mf;       /* Matched filter for 4160 Hz */
  su_sampler_t resampler; /* Resampler */

  /* The following objects work at a 4160 rate */
  SUSCOUNT count;

  SUCOMPLEX samp_buffer[SU_APT_BUFF_LEN];
  unsigned int samp_ptr;
  SUSCOUNT samp_epoch;

  SUFLOAT mean_i;
  SUFLOAT mean_q;

  /* Correlator data */
  SUFLOAT sync_snr;
  SUCOMPLEX sync_fft[SU_APT_BUFF_LEN];
  SUCOMPLEX corr_fft[SU_APT_BUFF_LEN];
  SU_FFTW(_plan) direct_plan;
  SU_FFTW(_plan) reverse_plan;

  /* Sync detection */
  SUSCOUNT last_sync;
  SUSCOUNT next_sync;
  SUSCOUNT next_search;
  SUFLOAT last_sync_delta;

  /* Line buffer */
  SUFLOAT line_buffer[SU_APT_LINE_BUFF_LEN];
  SUSCOUNT last_epoch;
  unsigned int line_ptr;
  unsigned int line_last_samp;

  /* Image */
  PTR_LIST(uint8_t, scan_line);

  SUSCOUNT lines;
  SUFLOAT line_len_alpha;
  SUFLOAT line_len;
  SUFLOAT mean_black;
  SUFLOAT mean_white;
  SUBOOL have_levels;

  struct sigutils_apt_decoder_callbacks callbacks;
};

typedef struct sigutils_apt_decoder su_apt_decoder_t;

su_apt_decoder_t *su_apt_decoder_new(
    SUFLOAT fs,
    const struct sigutils_apt_decoder_callbacks *);

SUBOOL su_apt_decoder_feed_ex(
    su_apt_decoder_t *self,
    SUBOOL phase_only,
    const SUCOMPLEX *buffer,
    SUSCOUNT count);

SUBOOL su_apt_decoder_feed(
    su_apt_decoder_t *self,
    const SUCOMPLEX *buffer,
    SUSCOUNT count);

void su_apt_decoder_clear_image(su_apt_decoder_t *self);

void su_apt_decoder_set_snr(su_apt_decoder_t *self, SUFLOAT);

SUBOOL su_apt_decoder_dump_pgm(const su_apt_decoder_t *self, const char *path);

void su_apt_decoder_destroy(su_apt_decoder_t *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_SPECIFIC_APT_H */
