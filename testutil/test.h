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

#define SU_SIGBUF_SAMPLING_FREQUENCY_DEFAULT 8000

enum sigutils_dump_format {
  SU_DUMP_FORMAT_NONE,
  SU_DUMP_FORMAT_MATLAB,
  SU_DUMP_FORMAT_RAW,
  SU_DUMP_FORMAT_WAV
};

enum sigutils_test_time_units {
  SU_TIME_UNITS_UNDEFINED,
  SU_TIME_UNITS_USEC,
  SU_TIME_UNITS_MSEC,
  SU_TIME_UNITS_SEC,
  SU_TIME_UNITS_MIN,
  SU_TIME_UNITS_HOUR
};

struct sigutils_sigbuf {
  char    *name;       /* Buffer name */
  SUSCOUNT fs;         /* Sampling frequency */
  size_t   size;       /* Buffer size */
  SUBOOL   is_complex; /* Buffer type */

  union {
    void *buffer;
    SUCOMPLEX *as_complex;
    SUFLOAT   *as_float;
  };
};

typedef struct sigutils_sigbuf su_sigbuf_t;

struct sigutils_sigbuf_pool {
  char *name;
  SUSCOUNT fs; /* Default sampling frequency */
  PTR_LIST(su_sigbuf_t, sigbuf);
};

typedef struct sigutils_sigbuf_pool su_sigbuf_pool_t;

struct sigutils_test_entry;

struct su_test_run_params {
  SUSCOUNT buffer_size;
  SUSCOUNT fs;
  enum sigutils_dump_format dump_fmt;
};

struct sigutils_test_context {
  const struct su_test_run_params *params;
  const struct sigutils_test_entry *entry;
  su_sigbuf_pool_t *pool;
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

#define su_test_run_params_INITIALIZER \
{ \
  SU_TEST_SIGNAL_BUFFER_SIZE, /* buffer_size */ \
  SU_SIGBUF_SAMPLING_FREQUENCY_DEFAULT, /* fs */ \
  SU_DUMP_FORMAT_NONE /* dump_fmt */ \
}

#define SU_TEST_ENTRY(name) { STRINGIFY(name), name }

#define su_test_context_INITIALIZER \
{ \
  NULL, /* params */ \
  NULL, /* entry */ \
  NULL, /* pool */ \
  0, /* testno */ \
  {0, 0}, /* start */ \
  {0, 0}, /* end */ \
  0, /* elapsed_time */ \
  SU_TIME_UNITS_UNDEFINED /* time units */ \
}

#define SU_SYSCALL_ASSERT(expr)                                 \
  if ((expr) < 0) {                                             \
    SU_ERROR(                                                   \
          "Operation `%s' failed (negative value returned)\n",  \
          STRINGIFY(expr));                                     \
    goto fail;                                                  \
  }

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

SUFLOAT *su_test_ctx_getf_w_size(su_test_context_t *ctx, const char *name, SUSCOUNT size);

SUCOMPLEX *su_test_ctx_getc_w_size(su_test_context_t *ctx, const char *name, SUSCOUNT size);

SUFLOAT *su_test_ctx_getf(su_test_context_t *ctx, const char *name);

SUCOMPLEX *su_test_ctx_getc(su_test_context_t *ctx, const char *name);

SUBOOL su_test_run(
    const su_test_entry_t *test_list,
    unsigned int test_count,
    unsigned int range_start,
    unsigned int range_end,
    const struct su_test_run_params *params);

SUFLOAT *su_test_buffer_new(unsigned int size);

SUCOMPLEX *su_test_complex_buffer_new(unsigned int size);

SUFLOAT su_test_buffer_mean(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_std(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_pp(const SUFLOAT *buffer, unsigned int size);

SUFLOAT su_test_buffer_peak(const SUFLOAT *buffer, unsigned int size);

SUBOOL su_test_complex_buffer_dump_raw(
    const SUCOMPLEX *buffer,
    unsigned int size,
    const char *file);

SUBOOL su_test_buffer_dump_raw(
    const SUFLOAT *buffer,
    unsigned int size,
    const char *file);

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
    SUSCOUNT size);

SUBOOL su_test_ctx_dumpc(
    su_test_context_t *ctx,
    const char *name,
    const SUCOMPLEX *data,
    SUSCOUNT size);

SUBOOL su_test_ctx_resize_buf(
    su_test_context_t *ctx,
    const char *name,
    SUSCOUNT size);

void su_sigbuf_set_fs(su_sigbuf_t *sbuf, SUSCOUNT fs);

SUSCOUNT su_sigbuf_get_fs(const su_sigbuf_t *sbuf);

su_sigbuf_t *su_sigbuf_pool_lookup(su_sigbuf_pool_t *pool, const char *name);

SUBOOL su_sigbuf_resize(su_sigbuf_t *sbuf, SUSCOUNT size);

su_sigbuf_pool_t *su_sigbuf_pool_new(const char *name);

void su_sigbuf_pool_debug(const su_sigbuf_pool_t *pool);

SUFLOAT *su_sigbuf_pool_get_float(
    su_sigbuf_pool_t *pool,
    const char *name,
    SUSCOUNT size);

SUCOMPLEX *su_sigbuf_pool_get_complex(
    su_sigbuf_pool_t *pool,
    const char *name,
    SUSCOUNT size);

SUBOOL su_sigbuf_pool_helper_ensure_directory(const char *name);

SUBOOL su_sigbuf_pool_helper_dump_matlab(
    const void *data,
    SUSCOUNT size,
    SUBOOL is_complex,
    const char *directory,
    const char *name);

SUBOOL su_sigbuf_pool_helper_dump_wav(
    const void *data,
    SUSCOUNT size,
    SUSCOUNT fs,
    SUBOOL is_complex,
    const char *directory,
    const char *name);

SUBOOL su_sigbuf_pool_dump(
    const su_sigbuf_pool_t *pool,
    enum sigutils_dump_format f);

SUBOOL su_sigbuf_pool_dump_raw(const su_sigbuf_pool_t *pool);

SUBOOL su_sigbuf_pool_dump_matlab(const su_sigbuf_pool_t *pool);

SUBOOL su_sigbuf_pool_dump_wav(const su_sigbuf_pool_t *pool);

void su_sigbuf_pool_set_fs(su_sigbuf_pool_t *pool, SUSCOUNT fs);

SUSCOUNT su_sigbuf_pool_get_fs(const su_sigbuf_pool_t *pool);

void su_sigbuf_pool_destroy(su_sigbuf_pool_t *pool);

#endif /* _TEST_H */
