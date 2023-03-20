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

#ifndef _SIGUTILS_TVPROC_H
#define _SIGUTILS_TVPROC_H

#include "defs.h"
#include "iir.h"
#include "types.h"
#include "pll.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_tv_processor_params {
  /* Flags */
  SUBOOL enable_sync;
  SUBOOL reverse;
  SUBOOL interlace;
  SUBOOL enable_agc;
  SUBOOL enable_color;
  SUFLOAT x_off;

  /* Geometry */
  SUSCOUNT frame_lines;

  /* Filtering */
  SUBOOL enable_comb;
  SUBOOL comb_reverse;

  /* Timing */
  SUFLOAT hsync_len;
  SUFLOAT vsync_len;
  SUFLOAT line_len;
  SUSCOUNT vsync_odd_trigger;
  SUBOOL dominance;

  /* Chroma parameters */
  SUFLOAT chroma_fhint;
  SUFLOAT chroma_burst_start; /* Since the beginning of the line */
  SUFLOAT chroma_burst_len;

  /* Tolerances */
  SUFLOAT t_tol; /* Timing tolerance */
  SUFLOAT l_tol; /* Level tolerance */
  SUFLOAT g_tol; /* Geometry tolerance. */

  /* Error triggers */
  SUFLOAT hsync_huge_err; /* .25 */
  SUFLOAT hsync_max_err;  /* .15 */
  SUFLOAT hsync_min_err;  /* .1 */

  /* Time constants */
  SUFLOAT hsync_len_tau;        /* 9.5 */
  SUFLOAT hsync_fast_track_tau; /* 9.5 */
  SUFLOAT hsync_slow_track_tau;
  SUFLOAT line_len_tau; /* 9.5 */
  SUFLOAT chroma_res_tau;
  SUFLOAT agc_tau; /* 3 */
};

struct sigutils_pulse_finder {
  SUFLOAT base;
  SUFLOAT peak_thr;
  SUSCOUNT length;
  SUFLOAT level_tolerance;
  SUFLOAT time_tolerance;

  SUFLOAT last_y;

  su_iir_filt_t corr;
  SUBOOL present;
  SUFLOAT accum;
  SUFLOAT w_accum;
  SUFLOAT duration;
  SUFLOAT rel_pos;
};

typedef struct sigutils_pulse_finder su_pulse_finder_t;

SU_INSTANCER(
    su_pulse_finder,
    SUFLOAT base,
    SUFLOAT peak,
    SUSCOUNT len,
    SUFLOAT tolerance);
SU_COLLECTOR(su_pulse_finder);

SU_METHOD(su_pulse_finder, SUBOOL, feed, SUFLOAT x);
SU_GETTER(su_pulse_finder, SUFLOAT, get_pos);

struct sigutils_tv_frame_buffer {
  int width, height;
  SUFLOAT *buffer;
  SUCOMPLEX *chroma;
  struct sigutils_tv_frame_buffer *next;
};

typedef struct sigutils_tv_frame_buffer su_tv_frame_buffer_t;

SU_INSTANCER(
    su_tv_frame_buffer,
    const struct sigutils_tv_processor_params *params);

SU_COPY_INSTANCER(su_tv_frame_buffer);
SU_COLLECTOR(su_tv_frame_buffer);

enum sigutils_tv_processor_state {
  SU_TV_PROCESSOR_SEARCH,
  SU_TV_PROCESSOR_SYNCED,
};

struct sigutils_tv_pulse_separator {
  SUBOOL   search;       /* On if no pulse found */
  SUBOOL   syncing;      
  SUFLOAT alpha;         /* Used to filter out chroma */
  SUCOMPLEX y;           /* Result of the filtering */

  FILE *fp;

  SUSCOUNT n;
  SUSCOUNT n_prev;
  SUFLOAT  amp_gain_y;
  SUFLOAT  max_level;    /* AGC (using the same alpha) */
  SUSCOUNT gain_samples;
  SUSCOUNT gain_updates;
  SUBOOL   have_gain;
  SUFLOAT  line_gain;
  SUFLOAT  gain;
  SUSCOUNT line_count;

  SUSCOUNT retries;
  SUSCOUNT max_loc;
  
  SUBOOL   negative;     /* Whether we must invert the sign of the sync */
  
  SUFLOAT  sync_level;   /* Parameter: level used for sync. Everything above this, is ignored */
  SUFLOAT  sync_level_inv;
  SUFLOAT  sync_pwr;
  SUFLOAT  sync_accum;
  SUFLOAT  sync_err;
  SUFLOAT  sync_err_norm;
  SUSDIFF  sync_count;
  SUSCOUNT holdoff;
  SUSCOUNT sync_len;     /* To know for how long we have to integrate */
  SUSCOUNT window_len;   /* 2 * pulse_len */
  

  SUFLOAT  line_len_orig;
  SUFLOAT  line_len;     /* line length (samples, adjusted) */
  SUFLOAT  next_sync;    /* Prediction of where is the next sync to be found*/
  SUFLOAT  round_err;

  SUSDIFF search_start;  /* Where the next search starts */
  SUSCOUNT search_end;   /* Where it ends */
  SUFLOAT *history;
  SUFLOAT *shape;

  SUSCOUNT history_len; /* 1.5 * line_len */
  SUSCOUNT ptr;
};

typedef struct sigutils_tv_pulse_separator su_tv_pulse_separator_t;

SU_CONSTRUCTOR(su_tv_pulse_separator, SUFLOAT sync_len, SUFLOAT line_len, SUFLOAT sync_level);
SU_METHOD(su_tv_pulse_separator, SUBOOL, feed, SUCOMPLEX);
SU_DESTRUCTOR(su_tv_pulse_separator);


struct sigutils_tv_comb_filter {
  SUCOMPLEX *history;
  SUSCOUNT   history_len;
  SUSCOUNT   ptr;
  SUBOOL     negative;
};

typedef struct sigutils_tv_comb_filter su_tv_comb_filter_t;

SU_CONSTRUCTOR(su_tv_comb_filter, SUSCOUNT, SUBOOL);
SU_METHOD(su_tv_comb_filter, SUCOMPLEX, feed, SUCOMPLEX);
SU_DESTRUCTOR(su_tv_comb_filter);

struct sigutils_tv_processor {
  struct sigutils_tv_processor_params params;
  enum sigutils_tv_processor_state state;

  struct sigutils_tv_frame_buffer *free_pool;
  struct sigutils_tv_frame_buffer *current;

  su_tv_pulse_separator_t sync_sep;

  /* Sample counter */
  SUSCOUNT ptr;

  /* Precalculated data */
  SUSCOUNT field_lines;
  SUFLOAT agc_alpha;
  SUFLOAT pulse_alpha;
  SUFLOAT hsync_len_alpha;
  SUFLOAT hsync_slow_track_alpha;
  SUFLOAT hsync_fast_track_alpha;
  SUFLOAT line_len_alpha;

  /* Frame state */
  SUSCOUNT field_x;
  SUFLOAT field_x_dec;
  SUSCOUNT field_y;
  SUBOOL field_parity;
  SUBOOL field_complete;
  SUSCOUNT field_prev_ptr;
  SUSCOUNT frame_count;

  /* Comb filter's delay line */
  SUCOMPLEX *delay_line;
  SUSCOUNT   delay_line_len;
  SUSCOUNT   delay_line_ptr;
  SUCOMPLEX  delay_out_positive;
  SUCOMPLEX  delay_out_negative;

  /* AGC */
  SUFLOAT agc_gain;
  SUFLOAT agc_line_max;
  SUFLOAT agc_accum;
  SUSCOUNT agc_lines;

  /* Luma */
  SUFLOAT Y;

  /* Chroma */
  SUBOOL   chroma_sync;
  su_ncqo_t chroma_precenter;
  su_pll_t chroma_pll;
  su_iir_filt_t chroma_bandpass_i;
  su_iir_filt_t chroma_bandpass_q;
  su_tv_comb_filter_t chroma_comb;
  SUCOMPLEX chroma_x;
  SUFLOAT chroma_alpha;
  SUFLOAT chroma_gain;
  SUBOOL   line_parity;
  unsigned int line_count;
  SUBOOL    chroma_off;
  SUFLOAT chroma_acc;

  /* Pulse output */
  SUFLOAT pulse_x;

  /* Sync pulse detection */
  SUBOOL sync_found;
  SUSCOUNT sync_start;

  /* HSYNC detection */
  SUSCOUNT last_hsync;
  SUBOOL have_last_hsync;
  SUFLOAT est_hsync_len;
  SUBOOL hsync_slow_track;

  /* VSYNC detection */
  SUSCOUNT last_frame;
  SUSCOUNT last_vsync;
  SUSCOUNT vsync_counter;
  SUBOOL frame_has_vsync;

  /* Line length estimation */
  SUFLOAT est_line_len;
  SUFLOAT est_line_len_accum;
  SUSCOUNT est_line_len_count;

  FILE *chromafp;
};

typedef struct sigutils_tv_processor su_tv_processor_t;

SU_INSTANCER(
    su_tv_processor,
    const struct sigutils_tv_processor_params *params);
SU_COLLECTOR(su_tv_processor);

void su_tv_processor_params_pal(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate);

void su_tv_processor_params_ntsc(
    struct sigutils_tv_processor_params *self,
    SUFLOAT samp_rate);

SU_METHOD(
    su_tv_processor,
    SUBOOL,
    set_params,
    const struct sigutils_tv_processor_params *params);
SU_METHOD(su_tv_processor, SUBOOL, feed, SUCOMPLEX x);

SU_METHOD(su_tv_processor, su_tv_frame_buffer_t *, take_frame);
SU_METHOD(su_tv_processor, void, return_frame, su_tv_frame_buffer_t *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_TVPROC_H */
