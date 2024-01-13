/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

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

#include <sigutils/log.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sigutils/util/util.h>
#include <sigutils/util/compat-time.h>

#include <sigutils/types.h>

SUPRIVATE struct sigutils_log_config log_config;
SUPRIVATE uint32_t log_mask;
SUPRIVATE pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
SUPRIVATE SUBOOL su_forced_logging = SU_FALSE;
SUPRIVATE SUBOOL su_log_cr = SU_TRUE;
SUPRIVATE FILE  *su_log_fp = NULL;

void
su_log_mask_severity(enum sigutils_log_severity sev)
{
  log_mask |= (1 << sev);
}

void
su_log_unmask_severity(enum sigutils_log_severity sev)
{
  log_mask &= ~(1 << sev);
}

uint32_t
su_log_get_mask(void)
{
  return log_mask;
}

void
su_log_set_mask(uint32_t mask)
{
  log_mask = mask;
}

SUBOOL
su_log_is_masked(enum sigutils_log_severity sev)
{
  return !su_forced_logging && !!(log_mask & (1 << sev));
}

void
sigutils_log_message_destroy(struct sigutils_log_message *msg)
{
  if (msg->domain != NULL)
    free((void *)msg->domain);

  if (msg->function != NULL)
    free((void *)msg->function);

  if (msg->message != NULL)
    free((void *)msg->message);

  free(msg);
}

struct sigutils_log_message *
sigutils_log_message_dup(const struct sigutils_log_message *msg)
{
  struct sigutils_log_message *dup = NULL;

  if ((dup = calloc(1, sizeof(struct sigutils_log_message))) == NULL)
    goto fail;

  if ((dup->domain = strdup(msg->domain)) == NULL)
    goto fail;

  if ((dup->function = strdup(msg->domain)) == NULL)
    goto fail;

  if ((dup->message = strdup(msg->message)) == NULL)
    goto fail;

  dup->line = msg->line;
  dup->severity = msg->severity;
  dup->time = msg->time;

  return dup;

fail:
  sigutils_log_message_destroy(dup);

  return NULL;
}

const char *
su_log_severity_to_string(enum sigutils_log_severity sev)
{
  switch (sev) {
    case SU_LOG_SEVERITY_CRITICAL:
      return "Critical";

    case SU_LOG_SEVERITY_ERROR:
      return "Error";

    case SU_LOG_SEVERITY_WARNING:
      return "Warning";

    case SU_LOG_SEVERITY_INFO:
      return "Information";

    case SU_LOG_SEVERITY_DEBUG:
      return "Debug";
  }

  return "Unknown";
}

SUPRIVATE void
print_date(void)
{
  time_t t;
  struct tm tm;
  char mytime[50];

  time(&t);
  localtime_r(&t, &tm);

  strftime(mytime, sizeof(mytime), "%d %b %Y - %H:%M:%S", &tm);

  fprintf(su_log_fp, "%s", mytime);
}

SUPRIVATE void
su_log_func(void *private, const struct sigutils_log_message *msg)
{
  SUBOOL *cr = (SUBOOL *) private;
  SUBOOL is_except;
  size_t msglen;

  if (*cr) {
    switch (msg->severity) {
      case SU_LOG_SEVERITY_DEBUG:
        fprintf(su_log_fp, "\033[1;30m");
        print_date();
        fprintf(su_log_fp, " - debug: ");
        break;

      case SU_LOG_SEVERITY_INFO:
        print_date();
        fprintf(su_log_fp, " - ");
        break;

      case SU_LOG_SEVERITY_WARNING:
        print_date();
        fprintf(su_log_fp, " - \033[1;33mwarning [%s]\033[0m: ", msg->domain);
        break;

      case SU_LOG_SEVERITY_ERROR:
        print_date();

        is_except = 
             strstr(msg->message, "exception in \"") != NULL
          || strstr(msg->message, "failed to create instance") != NULL;

        if (is_except)
          fprintf(su_log_fp, "\033[1;30m   ");
        else
          fprintf(su_log_fp, " - \033[1;31merror   [%s]\033[0;1m: ", msg->domain);
        
        break;

      case SU_LOG_SEVERITY_CRITICAL:
        print_date();
        fprintf(su_log_fp, 
            " - \033[1;37;41mcritical[%s] in %s:%u\033[0m: ",
            msg->domain,
            msg->function,
            msg->line);
        break;
    }
  }

  msglen = strlen(msg->message);

  *cr = msg->message[msglen - 1] == '\n' || msg->message[msglen - 1] == '\r';

  fputs(msg->message, su_log_fp);

  if (*cr)
    fputs("\033[0m", su_log_fp);
}

void
su_log(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *function,
    unsigned int line,
    const char *message)
{
  struct sigutils_log_message msg = sigutils_log_message_INITIALIZER;
  SUBOOL log_needed = (log_config.log_func != NULL || su_forced_logging);

  if (!su_log_is_masked(sev) && log_needed) {
    gettimeofday(&msg.time, NULL);

    msg.severity = sev;
    msg.domain = domain;
    msg.function = function;
    msg.line = line;
    msg.message = message;

    if (log_config.exclusive)
      if (pthread_mutex_lock(&log_mutex) == -1) /* Too dangerous to log */
        return;

    if (log_config.log_func != NULL)
      (log_config.log_func)(log_config.priv, &msg);

    if (su_forced_logging)
      su_log_func(&su_log_cr, &msg);

    if (log_config.exclusive)
      (void)pthread_mutex_unlock(&log_mutex);
  }
}

void
su_logvprintf(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *function,
    unsigned int line,
    const char *msgfmt,
    va_list ap)
{
  char *msg = NULL;

  /*
   * Perform the burden of allocating a new message, if and only if this
   * severity is not masked
   */
  if (!su_log_is_masked(sev)) {
    if ((msg = vstrbuild(msgfmt, ap)) == NULL)
      goto done;

    su_log(sev, domain, function, line, msg);
  }

done:
  if (msg != NULL)
    free(msg);
}

void
su_logprintf(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *function,
    unsigned int line,
    const char *msgfmt,
    ...)
{
  va_list ap;

  if (!su_log_is_masked(sev)) {
    va_start(ap, msgfmt);

    su_logvprintf(sev, domain, function, line, msgfmt, ap);

    va_end(ap);
  }
}

void
su_log_init(const struct sigutils_log_config *config)
{
  const char *env  = getenv("SIGUTILS_FORCELOG");
  const char *logf = getenv("SIGUTILS_LOGFILE");
  FILE *fp;

  if (env != NULL && strlen(env) > 0) {
    su_forced_logging = SU_TRUE;

    if (logf != NULL)
      if ((fp = fopen(logf, "w")) != NULL)
        su_log_fp = fp;

    if (su_log_fp == NULL)
      su_log_fp = stdout;
  }

  log_config = *config;
}
