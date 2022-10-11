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

SUPRIVATE SUBOOL su_log_cr = SU_TRUE;
SUPRIVATE SUBOOL su_measure_ffts = SU_FALSE;
SUPRIVATE char *su_wisdom_file = NULL;

SUPRIVATE char
su_log_severity_to_char(enum sigutils_log_severity sev)
{
  const char *sevstr = "di!ex";

  if (sev < 0 || sev > SU_LOG_SEVERITY_CRITICAL)
    return '?';

  return sevstr[sev];
}

SUPRIVATE void
su_log_func_default(void *private, const struct sigutils_log_message *msg)
{
  SUBOOL *cr = (SUBOOL *)private;
  size_t msglen;

  if (*cr)
    fprintf(
        stderr,
        "[%c] %s:%u: ",
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
su_lib_is_using_wisdom(void)
{
  return su_measure_ffts;
}

SUBOOL
su_lib_set_wisdom_enabled(SUBOOL enabled)
{
  if (su_wisdom_file == NULL) {
    SU_ERROR("Not enabling FFT wisdom: wisdom file path not set\n");
    enabled = SU_FALSE;
  }

  su_measure_ffts = enabled;
}

SUBOOL
su_lib_set_wisdom_file(const char *cpath)
{
  SUBOOL ok = SU_FALSE;
  char *path = NULL;

  SU_FFTW(_forget_wisdom)();

  if (cpath != NULL) {
    SU_TRY(path = strbuild(cpath));
    if (!SU_FFTW(_import_wisdom_from_filename) (path)) {
      SU_INFO("No previous FFT wisdom found (yet)\n");
    }
  } else {
    su_measure_ffts = SU_FALSE;
  }

  if (su_wisdom_file != NULL)
    free(su_wisdom_file);

  su_wisdom_file = path;

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_lib_save_wisdom(void)
{
  if (su_wisdom_file != NULL)
    return SU_FFTW(_export_wisdom_to_filename) (su_wisdom_file);

  return SU_TRUE;
}

int
su_lib_fftw_strategy(void)
{
  return su_measure_ffts ? FFTW_MEASURE : FFTW_ESTIMATE;
}

SUBOOL
su_lib_init_ex(const struct sigutils_log_config *logconfig)
{
  unsigned int i = 0;

  if (logconfig == NULL)
    logconfig = &su_lib_log_config;

  su_log_init(logconfig);

  return SU_TRUE;
}

SUBOOL
su_lib_init(void)
{
  return su_lib_init_ex(NULL);
}
