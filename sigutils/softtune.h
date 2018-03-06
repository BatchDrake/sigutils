/*

  Copyright (C) 2016 Gonzalo Jose Carracedo Carballal

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

#ifndef _SIGUTILS_SOFTTUNE_H
#define _SIGUTILS_SOFTTUNE_H

#include "block.h"
#include "sigutils.h"
#include "ncqo.h"
#include "sampling.h"
#include "iir.h"

/* Extra bandwidth given to antialias filter */
#define SU_SOFTTUNER_ANTIALIAS_EXTRA_BW 2
#define SU_SOFTTUNER_ANTIALIAS_ORDER    4

struct sigutils_channel {
  SUFLOAT fc;    /* Channel central frequency */
  SUFLOAT f_lo;  /* Lower frequency belonging to the channel */
  SUFLOAT f_hi;  /* Upper frequency belonging to the channel */
  SUFLOAT bw;    /* Equivalent bandwidth */
  SUFLOAT snr;   /* Signal-to-noise ratio */
  SUFLOAT S0;    /* Peak signal power */
  SUFLOAT N0;    /* Noise level */
  SUFLOAT ft;    /* Tuner frequency */
  unsigned int age;     /* Channel age */
  unsigned int present; /* Is channel present? */
};

struct sigutils_softtuner_params {
  SUSCOUNT samp_rate;
  SUSCOUNT decimation;
  SUFLOAT  fc;
  SUFLOAT  bw;
};

#define sigutils_softtuner_params_INITIALIZER   \
{                                               \
  0, /* samp_rate */                            \
  0, /* decimation */                           \
  0, /* fc */                                   \
  0, /* bw */                                   \
}

struct sigutils_softtuner {
  struct sigutils_softtuner_params params;
  su_ncqo_t lo; /* Local oscillator */
  su_iir_filt_t antialias; /* Antialiasing filter */
  su_stream_t output; /* Output stream */
  su_off_t read_ptr;
  SUSCOUNT decim_ptr;
  SUBOOL filtered;
};

typedef struct sigutils_softtuner su_softtuner_t;

SUINLINE void
su_channel_detector_set_fc(su_softtuner_t *cd, SUFLOAT fc)
{
  cd->params.fc = fc;

  su_ncqo_init(
      &cd->lo,
      SU_ABS2NORM_FREQ(cd->params.samp_rate, cd->params.fc));
}

void su_softtuner_params_adjust_to_channel(
    struct sigutils_softtuner_params *params,
    const struct sigutils_channel *channel);

SUBOOL su_softtuner_init(
    su_softtuner_t *tuner,
    const struct sigutils_softtuner_params *params);

SUSCOUNT su_softtuner_feed(
    su_softtuner_t *tuner,
    const SUCOMPLEX *input,
    SUSCOUNT size);

SUSDIFF su_softtuner_read(
    su_softtuner_t *tuner,
    SUCOMPLEX *out,
    SUSCOUNT size);

void su_softtuner_finalize(su_softtuner_t *tuner);

#endif /* _SIGUTILS_SOFTTUNE_H */
