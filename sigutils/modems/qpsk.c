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

#include <stdlib.h>
#include <string.h>

#define SU_LOG_LEVEL "qpsk-modem"

#include "agc.h"
#include "log.h"
#include "modem.h"
#include "pll.h"
#include "sampling.h"

#define SU_QPSK_MODEM_COSTAS_LOOP_ARM_FILTER_ORDER 3
#define SU_QPSK_MODEM_ARM_BANDWIDTH_FACTOR 2
#define SU_QPSK_MODEM_LOOP_BANDWIDTH_FACTOR 1e-1
#define SU_QPSK_MODEM_GAIN_DEFAULT .70711
#define SU_QPSK_MODEM_RRC_ROLL_OFF_DEFAULT .25
#define SU_QPSK_MODEM_RRC_SPAN_DEFAULT 6
#define SU_QPSK_MODEM_SYMBOL_QUEUE_SIZE 256

struct qpsk_modem {
  SUSCOUNT fs; /* Sampling frequency */

  SUFLOAT  baud;
  SUFLOAT  arm_bw;  /* Arm filter bandwidth */
  SUFLOAT  loop_bw; /* Loop bandwidth */
  SUSCOUNT mf_span; /* RRC filter span in symbols */
  SUFLOAT  rolloff; /* Rolloff factor */
  SUFLOAT  fc;      /* Carrier frequency */

  SUBOOL abc; /* Enable Automatic Baudrate Control */
  SUBOOL afc; /* Enable Automatic Frequency Control */

  /* Property references */
  SUFLOAT *fc_ref;

  /* Blocks */
  su_block_t *cdr_block;
  su_block_t *costas_block;
  su_block_t *agc_block;
  su_block_t *rrc_block;

  su_block_port_t port;
};

void su_qpsk_modem_dtor(void *private)
{
  free(private);
}

#define SU_QPSK_MODEM_CREATE_BLOCK(dest, expr)           \
  if ((dest = expr) == NULL) {                           \
    SU_ERROR("operation failed: " STRINGIFY(expr) "\n"); \
    goto fail;                                           \
  }                                                      \
                                                         \
  if (!su_modem_register_block(modem, dest)) {           \
    SU_ERROR("failed to register block\n");              \
    su_block_destroy(dest);                              \
    goto fail;                                           \
  }

#define SU_QPSK_MODEM_INT_PROPERTY(dest, name)                               \
  if (!su_modem_expose_state_property(modem,                                 \
                                      name,                                  \
                                      SU_PROPERTY_TYPE_INTEGER,              \
                                      SU_TRUE,                               \
                                      &dest)) {                              \
    SU_ERROR("cannot initialize modem: can't expose `%s' property\n", name); \
    goto fail;                                                               \
  }

#define SU_QPSK_MODEM_FLOAT_PROPERTY(dest, name)                             \
  if (!su_modem_expose_state_property(modem,                                 \
                                      name,                                  \
                                      SU_PROPERTY_TYPE_FLOAT,                \
                                      SU_TRUE,                               \
                                      &dest)) {                              \
    SU_ERROR("cannot initialize modem: can't expose `%s' property\n", name); \
    goto fail;                                                               \
  }

#define SU_QPSK_MODEM_BOOL_PROPERTY(dest, name)                              \
  if (!su_modem_expose_state_property(modem,                                 \
                                      name,                                  \
                                      SU_PROPERTY_TYPE_BOOL,                 \
                                      SU_TRUE,                               \
                                      &dest)) {                              \
    SU_ERROR("cannot initialize modem: can't expose `%s' property\n", name); \
    goto fail;                                                               \
  }

SUBOOL
su_qpsk_modem_ctor(su_modem_t *modem, void **private)
{
  struct qpsk_modem *new           = NULL;
  struct su_agc_params agc_params  = su_agc_params_INITIALIZER;
  SUFLOAT             *rrc_gain    = NULL;
  SUFLOAT             *cdr_alpha   = NULL;
  SUFLOAT             *cdr_beta    = NULL;
  SUFLOAT             *costas_beta = NULL;

  if ((new = calloc(1, sizeof(struct qpsk_modem))) == NULL)
    goto fail;

  SU_QPSK_MODEM_INT_PROPERTY(new->fs, "samp_rate");
  SU_QPSK_MODEM_INT_PROPERTY(new->mf_span, "mf_span");
  SU_QPSK_MODEM_BOOL_PROPERTY(new->abc, "abc");
  SU_QPSK_MODEM_BOOL_PROPERTY(new->afc, "afc");

  SU_QPSK_MODEM_FLOAT_PROPERTY(new->baud, "baud");
  SU_QPSK_MODEM_FLOAT_PROPERTY(new->rolloff, "rolloff");
  SU_QPSK_MODEM_FLOAT_PROPERTY(new->fc, "fc");

  if (!su_modem_load_all_state_properties(modem)) {
    SU_ERROR("cannot initialize modem: failed to load mandatory properties\n");
    goto fail;
  }

  new->arm_bw  = SU_QPSK_MODEM_ARM_BANDWIDTH_FACTOR *new->baud;
  new->loop_bw = SU_QPSK_MODEM_LOOP_BANDWIDTH_FACTOR *new->baud;

  agc_params.delay_line_size  = 10;
  agc_params.mag_history_size = 10;
  agc_params.fast_rise_t      = 2;
  agc_params.fast_fall_t      = 4;

  agc_params.slow_rise_t = 20;
  agc_params.slow_fall_t = 40;

  agc_params.threshold = SU_DB(2e-2);

  agc_params.hang_max     = 30;
  agc_params.slope_factor = 0;

  /* Create blocks */
  SU_QPSK_MODEM_CREATE_BLOCK(new->agc_block, su_block_new("agc", &agc_params));

  SU_QPSK_MODEM_CREATE_BLOCK(
      new->costas_block,
      su_block_new("costas",
                   SU_COSTAS_KIND_QPSK,
                   SU_ABS2NORM_FREQ(new->fs, new->fc),
                   SU_ABS2NORM_FREQ(new->fs, new->arm_bw),
                   SU_QPSK_MODEM_COSTAS_LOOP_ARM_FILTER_ORDER,
                   SU_ABS2NORM_FREQ(new->fs, new->loop_bw)));

  SU_QPSK_MODEM_CREATE_BLOCK(
      new->rrc_block,
      su_block_new(
          "rrc",
          (unsigned int)(new->mf_span *SU_T2N_FLOAT(new->fs, 1. / new->baud)),
          SU_T2N_FLOAT(new->fs, 1. / new->baud),
          new->rolloff));

  SU_QPSK_MODEM_CREATE_BLOCK(new->cdr_block,
                             su_block_new("cdr",
                                          (SUFLOAT)1.,
                                          SU_ABS2NORM_BAUD(new->fs, new->baud),
                                          SU_QPSK_MODEM_SYMBOL_QUEUE_SIZE));

  /* Expose some properties */
  if ((new->fc_ref = su_block_get_property_ref(new->costas_block,
                                               SU_PROPERTY_TYPE_FLOAT,
                                               "f")) == NULL) {
    SU_ERROR("Cannot find f property in Costas block\n");
    goto fail;
  }

  /* Tweak others properties */
  if ((rrc_gain = su_block_get_property_ref(new->rrc_block,
                                            SU_PROPERTY_TYPE_FLOAT,
                                            "gain")) == NULL) {
    SU_ERROR("Cannot find gain property in RRC block\n");
    goto fail;
  }

  if ((cdr_alpha = su_block_get_property_ref(new->cdr_block,
                                             SU_PROPERTY_TYPE_FLOAT,
                                             "alpha")) == NULL) {
    SU_ERROR("Cannot find alpha property in CDR block\n");
    goto fail;
  }

  if ((cdr_beta = su_block_get_property_ref(new->cdr_block,
                                            SU_PROPERTY_TYPE_FLOAT,
                                            "beta")) == NULL) {
    SU_ERROR("Cannot find beta property in CDR block\n");
    goto fail;
  }

  if ((costas_beta = su_block_get_property_ref(new->costas_block,
                                               SU_PROPERTY_TYPE_FLOAT,
                                               "beta")) == NULL) {
    SU_ERROR("Cannot find beta property in Costas block\n");
    goto fail;
  }

  *rrc_gain = 5;
  *cdr_alpha *= .75;

  if (!new->abc)
    *cdr_beta = 0; /* Disable baudrate control */

  if (!new->afc)
    *costas_beta = 0; /* Disable frequency control */

  /* Plug everything */
  if (!su_modem_plug_to_source(modem, new->agc_block))
    goto fail;

  if (!su_block_plug(new->agc_block, 0, 0, new->costas_block))
    goto fail;

  if (!su_block_plug(new->costas_block, 0, 0, new->rrc_block))
    goto fail;

  if (!su_block_plug(new->rrc_block, 0, 0, new->cdr_block))
    goto fail;

  if (!su_block_port_plug(&new->port, new->cdr_block, 0))
    goto fail;

  *private = new;

  return SU_TRUE;

fail:
  if (new != NULL)
    su_qpsk_modem_dtor(new);

  return SU_FALSE;
}

SUBOOL
su_qpsk_modem_onpropertychanged(void *private, const su_modem_property_t *prop)
{
  return SU_TRUE;
}

SUPRIVATE void su_qpsk_modem_update_state(struct qpsk_modem *modem)
{
  modem->fc = SU_NORM2ABS_FREQ(modem->fs, *modem->fc_ref);
}

SUCOMPLEX
su_qpsk_modem_read_sample(su_modem_t *modem, void *private)
{
  struct qpsk_modem *qpsk_modem = (struct qpsk_modem *)private;
  SUSDIFF            got        = 0;
  SUCOMPLEX          sample;

  if ((got = su_block_port_read(&qpsk_modem->port, &sample, 1)) == 0)
    return nan("nosym");
  else if (got < 0)
    return nan("eos");

  su_qpsk_modem_update_state(qpsk_modem);

  return sample;
}

SUSYMBOL
su_qpsk_modem_read_sym(su_modem_t *modem, void *private)
{
  struct qpsk_modem *qpsk_modem = (struct qpsk_modem *)private;
  SUSDIFF            got        = 0;
  SUCOMPLEX          sample;
  SUSYMBOL           sym = 0;

  if ((got = su_block_port_read(&qpsk_modem->port, &sample, 1)) == 0)
    return SU_NOSYMBOL;
  else if (got < 0)
    return SU_EOS;

  sym = 3 & (SUSYMBOL)floor(2 * (SU_C_ARG(sample) + M_PI) / M_PI);

  su_qpsk_modem_update_state(qpsk_modem);

  return sym + 1;
}

struct sigutils_modem_class su_modem_class_QPSK = {
    "qpsk",                          /* name */
    su_qpsk_modem_ctor,              /* ctor */
    su_qpsk_modem_read_sym,          /* read_sym */
    su_qpsk_modem_read_sample,       /* read_sample */
    su_qpsk_modem_onpropertychanged, /* onpropertychanged */
    su_qpsk_modem_dtor,              /* dtor */
};
