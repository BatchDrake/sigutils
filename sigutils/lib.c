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

#include <string.h>

#define SU_LOG_LEVEL "lib"

#include "sigutils.h"

/* Block classes */
extern struct sigutils_block_class su_block_class_AGC;
extern struct sigutils_block_class su_block_class_TUNER;
extern struct sigutils_block_class su_block_class_WAVFILE;
extern struct sigutils_block_class su_block_class_COSTAS;
extern struct sigutils_block_class su_block_class_RRC;
extern struct sigutils_block_class su_block_class_CDR;
extern struct sigutils_block_class su_block_class_SIGGEN;

/* Modem classes */
extern struct sigutils_modem_class su_modem_class_QPSK;

/* Encoder classes */
extern struct sigutils_codec_class su_codec_class_DIFF;

SUPRIVATE SUBOOL su_log_cr = SU_TRUE;

SUPRIVATE char su_log_severity_to_char(enum sigutils_log_severity sev)
{
  const char *sevstr = "di!ex";

  if (sev < 0 || sev > SU_LOG_SEVERITY_CRITICAL)
    return '?';

  return sevstr[sev];
}

SUPRIVATE void su_log_func_default(void *private,
                                   const struct sigutils_log_message *msg)
{
  SUBOOL *cr = (SUBOOL *)private;
  size_t  msglen;

  if (*cr)
    fprintf(stderr,
            "[%c] %s:%d: ",
            su_log_severity_to_char(msg->severity),
            msg->function,
            msg->line);

  msglen = strlen(msg->message);

  *cr = msg->message[msglen - 1] == '\n' || msg->message[msglen - 1] == '\r';

  fputs(msg->message, stderr);
}

/* Log config */
SUPRIVATE struct sigutils_log_config su_lib_log_config = {
    &su_log_cr,          /* private */
    SU_TRUE,             /* exclusive */
    su_log_func_default, /* log_func */
};

SUBOOL
su_lib_init_ex(const struct sigutils_log_config *logconfig)
{
  unsigned int i = 0;

  struct sigutils_block_class *blocks[] = {
      &su_block_class_AGC,
      &su_block_class_TUNER,
      &su_block_class_WAVFILE,
      &su_block_class_COSTAS,
      &su_block_class_RRC,
      &su_block_class_CDR,
      &su_block_class_SIGGEN,
  };

  struct sigutils_modem_class *modems[] = {&su_modem_class_QPSK};

  struct sigutils_codec_class *codecs[] = {&su_codec_class_DIFF};

  if (logconfig == NULL)
    logconfig = &su_lib_log_config;

  su_log_init(logconfig);

  for (i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i)
    if (!su_block_class_register(blocks[i])) {
      if (blocks[i]->name != NULL)
        SU_ERROR("Failed to register block class `%s'\n", blocks[i]->name);
      return SU_FALSE;
    }

  for (i = 0; i < sizeof(modems) / sizeof(modems[0]); ++i)
    if (!su_modem_class_register(modems[i])) {
      if (modems[i]->name != NULL)
        SU_ERROR("Failed to register modem class `%s'\n", modems[i]->name);
      return SU_FALSE;
    }

  for (i = 0; i < sizeof(codecs) / sizeof(codecs[0]); ++i)
    if (!su_codec_class_register(codecs[i])) {
      if (codecs[i]->name != NULL)
        SU_ERROR("Failed to register codec class `%s'\n", codecs[i]->name);
      return SU_FALSE;
    }

  return SU_TRUE;
}

SUBOOL
su_lib_init(void)
{
  return su_lib_init_ex(NULL);
}
