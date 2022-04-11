/*
  Copyright (C) 2022 Ángel Ruiz Fernández

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

#ifndef _UTIL_TIME_H
#define _UTIL_TIME_H

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#define gmtime_r(timep, result) gmtime_s(result, timep)
#define localtime_r(timep, result) localtime_s(result, timep)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* timeradd, timersub, timercmp were borrowed from the
   Standard GNU C Library */
#ifndef timercmp
#  define timercmp(a, b, CMP)                                      \
    (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP(b)->tv_usec) \
                                  : ((a)->tv_sec CMP(b)->tv_sec))
#endif /* timercmp */

#ifndef timeradd
#  define timeradd(a, b, result)                       \
    do {                                               \
      (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;    \
      (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
      if ((result)->tv_usec >= 1000000) {              \
        ++(result)->tv_sec;                            \
        (result)->tv_usec -= 1000000;                  \
      }                                                \
    } while (0)
#endif /* timeradd */

#ifndef timersub
#  define timersub(a, b, result)                       \
    do {                                               \
      (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
      (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
      if ((result)->tv_usec < 0) {                     \
        --(result)->tv_sec;                            \
        (result)->tv_usec += 1000000;                  \
      }                                                \
    } while (0)
#endif /* timersub */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _UTIL_TIME_H */
