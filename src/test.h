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

#ifndef _TEST_H
#define _TEST_H

#include <util.h>

#include <sigutils/types.h>
#include <sys/time.h>

#define SU_TEST_SIGNAL_BUFFER_SIZE 65536

enum sigutils_test_time_units {
  SU_TIME_UNITS_UNDEFINED,
  SU_TIME_UNITS_USEC,
  SU_TIME_UNITS_MSEC,
  SU_TIME_UNITS_SEC,
  SU_TIME_UNITS_MIN,
  SU_TIME_UNITS_HOUR
};

struct sigutils_test_context {
  unsigned int testno;
  struct timeval start;
  struct timeval end;
  float elapsed_time;
  enum sigutils_test_time_units time_units;
};

typedef struct sigutils_test_context su_test_context_t;

#define su_test_context_INITIALIZER            \
  {0, {0, 0}, {0, 0}, 0, SU_TIME_UNITS_UNDEFINED}

#define SU_TEST_START(ctx)                     \
  printf("[t:%3d] %s: start\n",                \
         ctx->testno,                          \
         __FUNCTION__);                        \
  gettimeofday(&ctx->start, NULL);             \

#define SU_TEST_START_TICKLESS(ctx)            \
  printf("[t:%3d] %s: start\n",                \
         ctx->testno,                          \
         __FUNCTION__);

#define SU_TEST_TICK(ctx)                      \
  gettimeofday(&ctx->start, NULL)              \


#define SU_TEST_END(ctx)                       \
  gettimeofday(&(ctx)->end, NULL);             \
  su_test_context_update_times(ctx);           \
  printf("[t:%3d] %s: end (%g %s)\n",          \
         ctx->testno,                          \
         __FUNCTION__,                         \
         ctx->elapsed_time,                    \
         su_test_context_time_units(ctx));     \

#define SU_TEST_ASSERT(cond)                   \
    if (!(cond)) {                             \
      printf("[t:%3d] %s: assertion failed\n", \
             ctx->testno,                      \
             __FUNCTION__);                    \
      printf("[t:%3d] %s: !(%s)\n",            \
             ctx->testno,                      \
             __FUNCTION__,                     \
             STRINGIFY(cond));                 \
      goto done;                               \
    }

typedef SUBOOL (*su_test_cb_t) (su_test_context_t *);

SUBOOL su_test_run(
    const su_test_cb_t *test_list,
    unsigned int test_count,
    unsigned int range_start,
    unsigned int range_end);

SUFLOAT *su_test_buffer_new(unsigned int size);

SUFLOAT su_test_buffer_mean(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_std(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_pp(const SUFLOAT *buffer, unsigned int size);

SUBOOL su_test_buffer_dump_matlab(
    const SUFLOAT *buffer,
    unsigned int size,
    const char *file,
    const char *arrname);

#endif /* _TEST_H */
