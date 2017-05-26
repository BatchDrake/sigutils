/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "types.h"
#include "log.h"
#include <util.h>

SUPRIVATE struct sigutils_log_config log_config;
SUPRIVATE uint32_t log_mask;
SUPRIVATE pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

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
  return !!(log_mask & (1 << sev));
}

void
sigutils_log_message_destroy(struct sigutils_log_message *msg)
{
  if (msg->domain != NULL)
    free((void *) msg->domain);

  if (msg->function != NULL)
    free((void *) msg->function);

  if (msg->message != NULL)
    free((void *) msg->message);

  free(msg);
}

struct sigutils_log_message *
sigutils_log_message_dup(const struct sigutils_log_message *msg)
{
  struct sigutils_log_message *dup = NULL;

  if ((dup = calloc(1, sizeof (struct sigutils_log_message))) == NULL)
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

void
su_log(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *function,
    unsigned int line,
    const char *message)
{
  struct sigutils_log_message msg = sigutils_log_message_INITIALIZER;

  if (!su_log_is_masked(sev) && log_config.log_func != NULL) {
    gettimeofday(&msg.time, NULL);

    msg.severity = sev;
    msg.domain   = domain;
    msg.function = function;
    msg.line     = line;
    msg.message  = message;

    if (log_config.exclusive)
      if (pthread_mutex_lock(&log_mutex) == -1) /* Too dangerous to log */
        return;

    (log_config.log_func) (log_config.private, &msg);

    if (log_config.exclusive)
      (void) pthread_mutex_unlock(&log_mutex);
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
  log_config = *config;
}

