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

enum sigutils_test_time_units {
  SU_TIME_UNITS_UNDEFINED,
  SU_TIME_UNITS_USEC,
  SU_TIME_UNITS_MSEC,
  SU_TIME_UNITS_SEC,
  SU_TIME_UNITS_MIN,
  SU_TIME_UNITS_HOUR
};

struct sigutils_sigbuf {
  char *name;
  size_t size;
  SUBOOL is_complex;

  union {
    void *buffer;
    SUCOMPLEX *as_complex;
    SUFLOAT   *as_float;
  };
};

typedef struct sigutils_sigbuf su_sigbuf_t;

struct sigutils_sigbuf_pool {
  char *name;
  PTR_LIST(su_sigbuf_t, sigbuf);
};

typedef struct sigutils_sigbuf_pool su_sigbuf_pool_t;

struct sigutils_test_entry;

struct sigutils_test_context {
  SUBOOL dump_results;
  const struct sigutils_test_entry *entry;
  su_sigbuf_pool_t *pool;
  size_t buffer_size;
  unsigned int testno;
  struct timeval start;
  struct timeval end;
  float elapsed_time;
  enum sigutils_test_time_units time_units;
};

typedef struct sigutils_test_context su_test_context_t;

typedef SUBOOL (*su_test_cb_t) (su_test_context_t *);

struct sigutils_test_entry {
  const char *name;
  su_test_cb_t cb;
};

typedef struct sigutils_test_entry su_test_entry_t;

#define SU_TEST_ENTRY(name) { STRINGIFY(name), name }

#define su_test_context_INITIALIZER            \
  {SU_FALSE, NULL, NULL, 0, 0, {0, 0}, {0, 0}, 0, SU_TIME_UNITS_UNDEFINED}

#define SU_TEST_START(ctx)                     \
  printf("[t:%3d] %s: start\n",                \
         ctx->testno,                          \
         ctx->entry->name);                    \
  gettimeofday(&ctx->start, NULL);             \

#define SU_TEST_START_TICKLESS(ctx)            \
  printf("[t:%3d] %s: start\n",                \
         ctx->testno,                          \
         ctx->entry->name);

#define SU_TEST_TICK(ctx)                      \
  gettimeofday(&ctx->start, NULL)              \


#define SU_TEST_END(ctx)                       \
  gettimeofday(&(ctx)->end, NULL);             \
  su_test_context_update_times(ctx);           \
  printf("[t:%3d] %s: end (%g %s)\n",          \
         ctx->testno,                          \
         ctx->entry->name,                     \
         ctx->elapsed_time,                    \
         su_test_context_time_units(ctx));     \

#define SU_TEST_ASSERT(cond)                   \
    if (!(cond)) {                             \
      printf("[t:%3d] %s: assertion failed\n", \
             ctx->testno,                      \
             ctx->entry->name);                \
      printf("[t:%3d] %s: !(%s)\n",            \
             ctx->testno,                      \
             ctx->entry->name,                 \
             STRINGIFY(cond));                 \
      goto done;                               \
    }


void su_test_context_update_times(su_test_context_t *ctx);

const char *su_test_context_time_units(const su_test_context_t *ctx);

SUFLOAT *su_test_ctx_getf_w_size(su_test_context_t *ctx, const char *name, size_t size);

SUCOMPLEX *su_test_ctx_getc_w_size(su_test_context_t *ctx, const char *name, size_t size);

SUFLOAT *su_test_ctx_getf(su_test_context_t *ctx, const char *name);

SUCOMPLEX *su_test_ctx_getc(su_test_context_t *ctx, const char *name);

SUBOOL su_test_run(
    const su_test_entry_t *test_list,
    unsigned int test_count,
    unsigned int range_start,
    unsigned int range_end,
    size_t buffer_size,
    SUBOOL save);

SUFLOAT *su_test_buffer_new(unsigned int size);

SUCOMPLEX *su_test_complex_buffer_new(unsigned int size);

SUFLOAT su_test_buffer_mean(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_std(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_pp(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_peak(const SUFLOAT *buffer, unsigned int size);

SUBOOL su_test_complex_buffer_dump_matlab(
    const SUCOMPLEX *buffer,
    unsigned int size,
    const char *file,
    const char *arrname);

SUBOOL su_test_buffer_dump_matlab(
    const SUFLOAT *buffer,
    unsigned int size,
    const char *file,
    const char *arrname);

SUBOOL su_test_ctx_dumpf(
    su_test_context_t *ctx,
    const char *name,
    const SUFLOAT *data,
    size_t size);

SUBOOL su_test_ctx_dumpc(
    su_test_context_t *ctx,
    const char *name,
    const SUCOMPLEX *data,
    size_t size);

su_sigbuf_pool_t *su_sigbuf_pool_new(const char *name);

SUFLOAT *su_sigbuf_pool_get_float(
    su_sigbuf_pool_t *pool,
    const char *name,
    size_t size);

SUCOMPLEX *su_sigbuf_pool_get_complex(
    su_sigbuf_pool_t *pool,
    const char *name,
    size_t size);

SUBOOL su_sigbuf_pool_helper_ensure_directory(const char *name);

SUBOOL su_sigbuf_pool_helper_dump(
    const void *data,
    size_t size,
    SUBOOL is_complex,
    const char *directory,
    const char *name);

SUBOOL su_sigbuf_pool_dump(su_sigbuf_pool_t *pool);

void su_sigbuf_pool_destroy(su_sigbuf_pool_t *pool);

#endif /* _TEST_H */
