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

#ifndef _SIGUTILS_TVPROC_H
#define _SIGUTILS_TVPROC_H

#include "types.h"
#include "iir.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_tv_processor_params {
  SUFLOAT  sync_level;
  SUFLOAT  black_level;
  SUFLOAT  white_level;
  SUBOOL   enable_comb;

  /* Timing */
  SUSCOUNT hsync_len;
  SUSCOUNT vsync_len;
  SUSCOUNT line_len;
  SUSCOUNT frame_len;
  SUSCOUNT vsync_pulses; /* 6 for NTSC, 5 for PAL */
  SUSCOUNT max_lines;

  /* Tolerances */
  SUFLOAT  t_tol; /* Timing tolerance */
  SUFLOAT  v_tol; /* Voltage tolerance */
};

/* Defaults assume 4 Msps arbitrarily */

#define sigutils_tv_processor_params_INITIALIZER \
{                                                \
  0,    /* sync_level */                         \
  .339, /* black_level */                        \
  1,    /* white_level */                        \
  SU_TRUE, /* enable_comb */                     \
  19,   /* hsync_len */                          \
  9,    /* vsync_len */                          \
  254,  /* line_len */                           \
  5e-2, /* t_tol */                              \
}

/*
 * Algorithm is as follows:
 *  - Look for an HSYNC and then a VSYNC along
 */

struct sigutils_pulse_finder {
  SUFLOAT  base;
  SUFLOAT  peak_thr;
  SUSCOUNT length;
  SUFLOAT  level_tolerance;
  SUFLOAT  time_tolerance;

  SUFLOAT  last_y;

  su_iir_filt_t corr;
  SUBOOL  present;
  SUFLOAT accum;
  SUFLOAT w_accum;
  SUFLOAT duration;
  SUFLOAT rel_pos;
};

typedef struct sigutils_pulse_finder su_pulse_finder_t;

su_pulse_finder_t *su_pulse_finder_new(
    SUFLOAT base,
    SUFLOAT peak,
    SUSCOUNT len,
    SUFLOAT tolerance);

SUBOOL  su_pulse_finder_feed(su_pulse_finder_t *self, SUFLOAT);

SUFLOAT su_pulse_finder_get_pos(const su_pulse_finder_t *self);

void su_pulse_finder_destroy(su_pulse_finder_t *self);

struct sigutils_tv_frame_buffer {
  int width, height;
  SUFLOAT *buffer;
  struct sigutils_tv_frame_buffer *next;
};

struct sigutils_tv_frame_buffer *su_tv_frame_buffer_new(
    const struct sigutils_tv_processor_params *);

void su_tv_frame_buffer_destroy(struct sigutils_tv_frame_buffer *);

enum sigutils_tv_processor_state {
  SU_TV_PROCESSOR_SEARCH,
  SU_TV_PROCESSOR_SYNCED,
};

struct sigutils_tv_processor {
  struct sigutils_tv_processor_params params;
  enum sigutils_tv_processor_state state;

  struct sigutils_tv_frame_buffer *free_pool;
  struct sigutils_tv_frame_buffer *current;

  /* Frame state */
  SUSCOUNT field_x;
  SUFLOAT  field_x_dec;
  SUSCOUNT field_y;
  SUBOOL   field_parity;

  su_pulse_finder_t *hsync_finder;
  su_pulse_finder_t *vsync_finder;

  /* Sample counter */
  SUSCOUNT ptr;

  /* Sync pulse detection */
  SUBOOL   sync_found;
  SUSCOUNT sync_start;

  /* HSYNC detection */
  SUSCOUNT last_hsync;
  SUBOOL   have_last_hsync;
  SUFLOAT  true_hsync_len;
  SUSCOUNT hsync_corr_count;

  /* VSYNC detection */
  SUSCOUNT last_vsync;
  SUBOOL   frame_synced;

  /* Line length estimation */
  SUFLOAT  true_line_len;
  SUFLOAT  true_line_len_accum;
  SUSCOUNT true_line_len_count;

  /* Pulse output */
  SUFLOAT  pulse_alpha;
  SUFLOAT  pulse_x;

  /* AGC */
  SUFLOAT  agc_gain;
  SUFLOAT  agc_line_max;
  SUFLOAT  agc_accum;
  SUSCOUNT agc_lines;

  /* Comb filter's delay line */
  SUFLOAT *delay_line;
  SUSCOUNT delay_line_ptr;

};

typedef struct sigutils_tv_processor su_tv_processor_t;

void su_tv_processor_params_pal(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate);

void su_tv_processor_params_ntsc(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate);

su_tv_processor_t *su_tv_processor_new(
    const struct sigutils_tv_processor_params *params);

SUBOOL su_tv_processor_set_params(
    su_tv_processor_t *self,
    const struct sigutils_tv_processor_params *params);

SUBOOL su_tv_processor_feed(
    su_tv_processor_t *self,
    SUFLOAT feed);

struct sigutils_tv_frame_buffer *su_tv_processor_take_frame(
    su_tv_processor_t *);

void su_tv_processor_return_frame(
    su_tv_processor_t *self,
    struct sigutils_tv_frame_buffer *);

void su_tv_processor_destroy(su_tv_processor_t *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_TVPROC_H */
