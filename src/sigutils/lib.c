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

#include <sigutils/sigutils.h>
#include <pthread.h>

#define SU_MIN_PRECALC_FFT_EXP 9  /* 512 bin FFT */
#define SU_MAX_PRECALC_FFT_EXP 20 /* 1M  bin FFT */

SUPRIVATE SUBOOL          g_fftw_init       = SU_FALSE;
SUPRIVATE SUBOOL          g_su_log_cr       = SU_TRUE;
SUPRIVATE SUBOOL          g_su_measure_ffts = SU_FALSE;
SUPRIVATE char           *g_su_wisdom_file  = NULL;
SUPRIVATE pthread_mutex_t g_fft_plan_mutex  = PTHREAD_MUTEX_INITIALIZER;

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
SUPRIVATE struct sigutils_log_config g_su_lib_log_config = {
    &g_su_log_cr,          /* private */
    SU_TRUE,             /* exclusive */
    su_log_func_default, /* log_func */
};

SUBOOL
su_lib_is_using_wisdom(void)
{
  return g_su_measure_ffts;
}

SUBOOL
su_lib_set_wisdom_enabled(SUBOOL enabled)
{
  if (g_su_wisdom_file == NULL) {
    SU_ERROR("Not enabling FFT wisdom: wisdom file path not set\n");
    enabled = SU_FALSE;
  }

  g_su_measure_ffts = enabled;

  return SU_TRUE;
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
    g_su_measure_ffts = SU_FALSE;
  }

  if (g_su_wisdom_file != NULL)
    free(g_su_wisdom_file);

  g_su_wisdom_file = path;

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_lib_save_wisdom(void)
{
  if (g_su_wisdom_file != NULL)
    return SU_FFTW(_export_wisdom_to_filename) (g_su_wisdom_file);

  return SU_TRUE;
}

int
su_lib_fftw_strategy(void)
{
  return g_su_measure_ffts ? FFTW_MEASURE : FFTW_ESTIMATE;
}

SU_FFTW(_plan)
su_lib_plan_dft_1d(int n, SU_FFTW(_complex) *in, SU_FFTW(_complex) *out,
        int sign, unsigned flags)
{
  SU_FFTW(_plan) plan = NULL;
  SUBOOL mutex_acquired = SU_FALSE;
  int nthreads;

  if (n < 32768)
    nthreads = 1;
  else if (n == 32768)
    nthreads = 2;
  else
    nthreads = 4;

  SU_TRYZ(pthread_mutex_lock(&g_fft_plan_mutex));
  mutex_acquired = SU_TRUE;

  SU_FFTW(_plan_with_nthreads)(nthreads);
  SU_TRY(plan = SU_FFTW(_plan_dft_1d)(n, in, out, sign, flags));
  
done:
  if (mutex_acquired) {
    SU_FFTW(_plan_with_nthreads)(1);
    pthread_mutex_unlock(&g_fft_plan_mutex);
  }

  return plan;
}

void
su_lib_gen_wisdom(void)
{
  unsigned e;
  int size;
  SU_FFTW(_complex) *buffer = NULL;

  SU_TRY(buffer = SU_FFTW(_malloc) ((1 << 20) * sizeof(SUCOMPLEX)));

  for (e = SU_MIN_PRECALC_FFT_EXP; e <= SU_MAX_PRECALC_FFT_EXP; ++e) {
    size = 1 << e;

    SU_FFTW(_plan) plan = su_lib_plan_dft_1d(
      size,
      buffer,
      buffer,
      FFTW_FORWARD,
      FFTW_MEASURE);
    
    if (plan != NULL)
      SU_FFTW(_destroy_plan) (plan);
  }

done:
  if (buffer != NULL)
    SU_FFTW(_free)(buffer);
}

SUBOOL
su_lib_init_ex(const struct sigutils_log_config *logconfig)
{
  if (logconfig != NULL)
    su_log_init(logconfig);

  if (!g_fftw_init) {
    SU_FFTW(_init_threads)();
    SU_FFTW(_make_planner_thread_safe)();
    g_fftw_init = SU_TRUE;
  }

  return SU_TRUE;
}

SUBOOL
su_lib_init(void)
{
  return su_lib_init_ex(&g_su_lib_log_config);
}
