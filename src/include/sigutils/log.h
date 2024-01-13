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

#ifndef _SIGUTILS_LOG_H
#define _SIGUTILS_LOG_H

#include <stdint.h>
#include <sys/time.h>

#include <sigutils/defs.h>
#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum sigutils_log_severity {
  SU_LOG_SEVERITY_DEBUG,
  SU_LOG_SEVERITY_INFO,
  SU_LOG_SEVERITY_WARNING,
  SU_LOG_SEVERITY_ERROR,
  SU_LOG_SEVERITY_CRITICAL
};

struct sigutils_log_message {
  enum sigutils_log_severity severity;
  struct timeval time;
  const char *domain;
  const char *function;
  unsigned int line;
  const char *message;
};

#define sigutils_log_message_INITIALIZER  \
  {                                       \
    SU_LOG_SEVERITY_DEBUG, /* severity */ \
        {0, 0},            /* time */     \
        NULL,              /* domain */   \
        NULL,              /* function */ \
        0,                 /* line */     \
        NULL,              /* message */  \
  }

struct sigutils_log_config {
  void *priv;
  SUBOOL exclusive;
  void (*log_func)(void *priv, const struct sigutils_log_message *msg);
};

#define sigutils_log_config_INITIALIZER \
  {                                     \
    NULL,        /* private */          \
        SU_TRUE, /* exclusive */        \
        NULL,    /* log_func */         \
  }

#ifndef SU_LOG_DOMAIN
#  define SU_LOG_DOMAIN __REL_FILE__
#endif /* SU_LOG_DOMAIN */

#define SU_ERROR(fmt, arg...) \
  su_logprintf(               \
      SU_LOG_SEVERITY_ERROR,  \
      SU_LOG_DOMAIN,          \
      __FUNCTION__,           \
      __LINE__,               \
      fmt,                    \
      ##arg)

#define SU_WARNING(fmt, arg...) \
  su_logprintf(                 \
      SU_LOG_SEVERITY_WARNING,  \
      SU_LOG_DOMAIN,            \
      __FUNCTION__,             \
      __LINE__,                 \
      fmt,                      \
      ##arg)

#define SU_INFO(fmt, arg...) \
  su_logprintf(              \
      SU_LOG_SEVERITY_INFO,  \
      SU_LOG_DOMAIN,         \
      __FUNCTION__,          \
      __LINE__,              \
      fmt,                   \
      ##arg)

void su_log_mask_severity(enum sigutils_log_severity sev);

void su_log_unmask_severity(enum sigutils_log_severity sev);

uint32_t su_log_get_mask(void);

void su_log_set_mask(uint32_t mask);

const char *su_log_severity_to_string(enum sigutils_log_severity sev);

SUBOOL su_log_is_masked(enum sigutils_log_severity sev);

void sigutils_log_message_destroy(struct sigutils_log_message *msg);

struct sigutils_log_message *sigutils_log_message_dup(
    const struct sigutils_log_message *msg);

void su_log(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *method,
    unsigned int line,
    const char *message);

void su_logvprintf(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *method,
    unsigned int line,
    const char *msgfmt,
    va_list ap);

void su_logprintf(
    enum sigutils_log_severity sev,
    const char *domain,
    const char *method,
    unsigned int line,
    const char *msgfmt,
    ...);

void su_log_init(const struct sigutils_log_config *config);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_LOG_H */
