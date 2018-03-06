/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

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
#include <stdlib.h>
#include <string.h>

#define SU_LOG_LEVEL "tuner-block"

#include "log.h"
#include "block.h"
#include "ncqo.h"
#include "iir.h"
#include "taps.h"

/* A tuner is just a NCQO + Low pass filter */
struct sigutils_tuner {
  su_iir_filt_t bpf;   /* Bandpass filter */
  su_ncqo_t  lo;       /* Local oscillator */
  SUFLOAT    if_off;   /* Intermediate frequency offset */

  /* Filter params */
  SUFLOAT bw;          /* Bandwidth */
  unsigned int h_size; /* Filter size */

  /* Configurable params */
  SUFLOAT      rq_bw;
  unsigned int rq_h_size;
  SUFLOAT      rq_if_off;
  SUFLOAT      rq_fc; /* Center frequency (1 ~ fs/2), hcps */
};

typedef struct sigutils_tuner su_tuner_t;

SUPRIVATE SUBOOL
su_tuner_filter_has_changed(su_tuner_t *tu)
{
  return
      tu->rq_bw != tu->bw         ||
      tu->rq_if_off != tu->if_off ||
      tu->rq_h_size != tu->h_size;
}

SUPRIVATE SUBOOL
su_tuner_lo_has_changed(su_tuner_t *tu)
{
  return su_ncqo_get_freq(&tu->lo) != tu->if_off - tu->rq_fc;
}

SUPRIVATE SUCOMPLEX
su_tuner_feed(su_tuner_t *tu, SUCOMPLEX samp)
{
  return su_iir_filt_feed(&tu->bpf, samp * su_ncqo_read(&tu->lo));
}

SUPRIVATE SUCOMPLEX
su_tuner_get(const su_tuner_t *tu)
{
  return su_iir_filt_get(&tu->bpf);
}

SUPRIVATE SUBOOL
su_tuner_update_filter(su_tuner_t *tu)
{
  su_iir_filt_t bpf_new = su_iir_filt_INITIALIZER;

  /* If baudrate has changed, we must change the LPF */
  if (!su_iir_brickwall_bp_init(
      &bpf_new,
      tu->rq_h_size,
      tu->rq_bw,
      tu->rq_if_off))
    goto fail;

  tu->bw     = tu->rq_bw;
  tu->h_size = tu->rq_h_size;
  tu->if_off = tu->rq_if_off;

  su_iir_filt_finalize(&tu->bpf);
  tu->bpf = bpf_new;

  return SU_TRUE;

fail:
  su_iir_filt_finalize(&bpf_new);

  return SU_FALSE;
}

SUPRIVATE void
su_tuner_update_lo(su_tuner_t *tu)
{
  su_ncqo_set_freq(&tu->lo, tu->if_off - tu->rq_fc);
}

void
su_tuner_destroy(su_tuner_t *tu)
{
  su_iir_filt_finalize(&tu->bpf);
  free(tu);
}

su_tuner_t *
su_tuner_new(SUFLOAT fc, SUFLOAT bw, SUFLOAT if_off, SUSCOUNT size)
{
  su_tuner_t *new;

  if ((new = calloc(1, sizeof (su_tuner_t))) == NULL)
    goto fail;

  new->rq_fc     = fc;
  new->rq_bw     = bw;
  new->rq_if_off = if_off;
  new->rq_h_size = size;

  if (!su_tuner_update_filter(new))
    goto fail;

  su_ncqo_init(&new->lo, new->rq_if_off - new->rq_fc);

  return new;

fail:
  if (new != NULL)
    su_tuner_destroy(new);

  return NULL;
}

/* Tuner constructor */
SUPRIVATE SUBOOL
su_block_tuner_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  su_tuner_t *tu = NULL;
  SUBOOL ok = SU_FALSE;
  SUFLOAT fc;
  SUFLOAT bw;
  SUFLOAT if_off;
  unsigned int size;
  unsigned int d;

  fc     = va_arg(ap, double);
  bw     = va_arg(ap, double);
  if_off = va_arg(ap, double);
  size   = va_arg(ap, SUSCOUNT);

  if ((tu = su_tuner_new(fc, bw, if_off, size)) == NULL)
    goto done;

  ok = SU_TRUE;

  /* Set configurable properties */
  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "bw",
      &tu->rq_bw);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "fc",
      &tu->rq_fc);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "if",
      &tu->rq_if_off);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_INTEGER,
      "size",
      &tu->rq_h_size);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_FLOAT,
      "taps",
      tu->bpf.b);

done:
  if (!ok) {
    if (tu != NULL)
      su_tuner_destroy(tu);
  }
  else
    *private = tu;

  return ok;
}

/* Tuner destructor */
SUPRIVATE void
su_block_tuner_dtor(void *private)
{
  su_tuner_t *tu = (su_tuner_t *) private;

  if (tu != NULL) {
    su_tuner_destroy(tu);
  }
}

/* Acquire */
SUPRIVATE SUSDIFF
su_block_tuner_acquire(
    void *priv,
    su_stream_t *out,
    unsigned int port_id,
    su_block_port_t *in)
{
  su_tuner_t *tu;
  SUSDIFF size;
  SUSDIFF got;
  int i = 0;

  SUCOMPLEX *start;

  tu  = (su_tuner_t *) priv;

  size = su_stream_get_contiguous(out, &start, out->size);

  do {
    if ((got = su_block_port_read(in, start, size)) > 0) {
      /* Got data, process in place */
      for (i = 0; i < got; ++i)
        start[i] = su_tuner_feed(tu, start[i]);

      /* Increment position */
      if (su_stream_advance_contiguous(out, got) != got) {
        SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
        return -1;
      }
    } else if (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC) {
      SU_WARNING("Tuner slow, samples lost\n");
      if (!su_block_port_resync(in)) {
        SU_ERROR("Failed to resync\n");
        return -1;
      }
    } else if (got < 0) {
      SU_ERROR("su_block_port_read: error %d\n", got);
      return -1;
    }
  } while (got == SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC);

  return got;
}

struct sigutils_block_class su_block_class_TUNER = {
    "tuner", /* name */
    1,       /* in_size */
    1,       /* out_size */
    su_block_tuner_ctor,    /* constructor */
    su_block_tuner_dtor,    /* destructor */
    su_block_tuner_acquire  /* acquire */
};

