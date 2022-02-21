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

#include <math.h>
#include <string.h>
#include <time.h>
#include <util/compat-time.h>
#include "test.h"

void
su_test_context_update_times(su_test_context_t *ctx)
{
  struct timeval diff;

  timersub(&ctx->end, &ctx->start, &diff);

  /* Hours */
  if (diff.tv_sec >= 3600) {
    ctx->elapsed_time = (diff.tv_sec + 1e-6 * diff.tv_usec) / 3600;
    ctx->time_units   = SU_TIME_UNITS_HOUR;
  } else if (diff.tv_sec >= 60) {
    ctx->elapsed_time = (diff.tv_sec + 1e-6 * diff.tv_usec) / 60;
    ctx->time_units   = SU_TIME_UNITS_MIN;
  } else if (diff.tv_sec >= 1) {
    ctx->elapsed_time = diff.tv_sec + 1e-6 * diff.tv_usec;
    ctx->time_units   = SU_TIME_UNITS_SEC;
  } else if (diff.tv_usec >= 1000) {
    ctx->elapsed_time = 1e-3 * diff.tv_usec;
    ctx->time_units   = SU_TIME_UNITS_MSEC;
  } else {
    ctx->elapsed_time = diff.tv_usec;
    ctx->time_units   = SU_TIME_UNITS_USEC;
  }
}

const char *
su_test_context_time_units(const su_test_context_t *ctx) {
  switch (ctx->time_units) {
    case SU_TIME_UNITS_HOUR:
      return "hour";

    case SU_TIME_UNITS_MIN:
      return "min";

    case SU_TIME_UNITS_SEC:
      return "s";

    case SU_TIME_UNITS_MSEC:
      return "ms";

    case SU_TIME_UNITS_USEC:
      return "us";

    case SU_TIME_UNITS_UNDEFINED:
      return "(no test executed)";
  }

  return "??";
}

SUFLOAT *
su_test_buffer_new(unsigned int size)
{
  return calloc(size, sizeof (SUFLOAT));
}

SUCOMPLEX *
su_test_complex_buffer_new(unsigned int size)
{
  return calloc(size, sizeof (SUCOMPLEX));
}

SUFLOAT
su_test_buffer_mean(const SUFLOAT *buffer, unsigned int size)
{
  SUFLOAT size_inv = 1. / size;
  SUFLOAT result = 0.;

  while (size--)
    result += size_inv * buffer[size];

  return result;
}

SUFLOAT
su_test_buffer_std(const SUFLOAT *buffer, unsigned int size)
{
  SUFLOAT size_inv = 1. / size;
  SUFLOAT result = 0.;
  SUFLOAT mean;

  mean = su_test_buffer_mean(buffer, size);

  while (size--)
    result += size_inv * (buffer[size] - mean) * (buffer[size] - mean);

  return sqrt(result);
}

SUFLOAT
su_test_buffer_peak(const SUFLOAT *buffer, unsigned int size)
{
  SUFLOAT max = 0;

  while (size--)
    if (max < SU_ABS(buffer[size]))
      max = SU_ABS(buffer[size]);

  return SU_DB(max);
}

SUFLOAT
su_test_buffer_pp(const SUFLOAT *buffer, unsigned int size)
{
  SUFLOAT min = INFINITY;
  SUFLOAT max = -INFINITY;

  while (size--) {
    if (buffer[size] < min)
      min = buffer[size];

    if (buffer[size] > max)
      max = buffer[size];
  }

  return max - min;
}

SUBOOL
su_test_complex_buffer_dump_raw(
    const SUCOMPLEX *buffer,
    unsigned int size,
    const char *file)
{
  FILE *fp = NULL;
  float val;
  unsigned int i;

  if ((fp = fopen(file, "wb")) == NULL)
    goto fail;

  for (i = 0; i < size; ++i) {
    val = SU_C_REAL(buffer[i]);
    if (fwrite(&val, sizeof (float), 1, fp) < 1)
      goto fail;
    val = SU_C_IMAG(buffer[i]);
    if (fwrite(&val, sizeof (float), 1, fp) < 1)
      goto fail;
  }

  fclose(fp);

  return SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}

SUBOOL
su_test_buffer_dump_raw(
    const SUFLOAT *buffer,
    unsigned int size,
    const char *file)
{
  FILE *fp = NULL;
  float val;
  unsigned int i;

  if ((fp = fopen(file, "wb")) == NULL)
    goto fail;

  for (i = 0; i < size; ++i) {
    val = buffer[i];
    if (fwrite(&val, sizeof (float), 1, fp) < 1)
      goto fail;
  }

  fclose(fp);

  return SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}

SUBOOL
su_test_complex_buffer_dump_matlab(
    const SUCOMPLEX *buffer,
    unsigned int size,
    const char *file,
    const char *arrname)
{
  FILE *fp = NULL;
  unsigned int i;

  if ((fp = fopen(file, "w")) == NULL)
    goto fail;

  if (fprintf(fp, "%s = [\n", arrname) < 0)
    goto fail;

  for (i = 0; i < size; ++i) {
    if (i > 0)
      if (fprintf(fp, ";\n") < 0)
        goto fail;

    if (fprintf(
        fp,
        SUFLOAT_PRECISION_FMT " + " SUFLOAT_PRECISION_FMT "i",
        SU_C_REAL(buffer[i]),
        SU_C_IMAG(buffer[i])) < 0)
      goto fail;
  }

  if (fprintf(fp, "\n];\n") < 0)
      goto fail;

  fclose(fp);

  return SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}

SUBOOL
su_test_buffer_dump_matlab(
    const SUFLOAT *buffer,
    unsigned int size,
    const char *file,
    const char *arrname)
{
  FILE *fp = NULL;
  unsigned int i;

  if ((fp = fopen(file, "w")) == NULL)
    goto fail;

  if (fprintf(fp, "%s = [\n", arrname) < 0)
    goto fail;

  for (i = 0; i < size; ++i) {
    if (i > 0)
      if (fprintf(fp, ";\n") < 0)
        goto fail;

    if (fprintf(fp, SUFLOAT_PRECISION_FMT, buffer[i]) < 0)
      goto fail;
  }

  if (fprintf(fp, "\n];\n") < 0)
      goto fail;

  fclose(fp);

  return SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}

SUFLOAT *
su_test_ctx_getf_w_size(su_test_context_t *ctx, const char *name, SUSCOUNT size)
{
  return su_sigbuf_pool_get_float(ctx->pool, name, size);
}

SUCOMPLEX *
su_test_ctx_getc_w_size(su_test_context_t *ctx, const char *name, SUSCOUNT size)
{
  return su_sigbuf_pool_get_complex(ctx->pool, name, size);
}

SUFLOAT *
su_test_ctx_getf(su_test_context_t *ctx, const char *name)
{
  return su_test_ctx_getf_w_size(ctx, name, ctx->params->buffer_size);
}

SUCOMPLEX *
su_test_ctx_getc(su_test_context_t *ctx, const char *name)
{
  return su_test_ctx_getc_w_size(ctx, name, ctx->params->buffer_size);
}

SUBOOL
su_test_ctx_resize_buf(su_test_context_t *ctx, const char *name, SUSCOUNT size)
{
  su_sigbuf_t *sbuf = NULL;

  if ((sbuf = su_sigbuf_pool_lookup(ctx->pool, name)) == NULL)
    return SU_FALSE;

  return su_sigbuf_resize(sbuf, size);
}

SUBOOL
su_test_ctx_dumpf(
    su_test_context_t *ctx,
    const char *name,
    const SUFLOAT *data,
    SUSCOUNT size)
{
  return su_sigbuf_pool_helper_dump_matlab(
      data,
      size,
      SU_FALSE,
      ctx->entry->name,
      name);
}

SUBOOL
su_test_ctx_dumpc(
    su_test_context_t *ctx,
    const char *name,
    const SUCOMPLEX *data,
    SUSCOUNT size)
{
  return su_sigbuf_pool_helper_dump_matlab(
      data,
      size,
      SU_TRUE,
      ctx->entry->name,
      name);
}

SUPRIVATE void
su_test_context_reset(su_test_context_t *ctx)
{
  if (ctx->pool != NULL)
    su_sigbuf_pool_destroy(ctx->pool);

  memset(ctx, 0, sizeof (su_test_context_t));

  ctx->time_units = SU_TIME_UNITS_UNDEFINED;
}

SUPRIVATE const char *
su_test_dump_format_to_string(enum sigutils_dump_format fmt)
{
  switch(fmt) {
    case SU_DUMP_FORMAT_NONE:
      return "none";

    case SU_DUMP_FORMAT_MATLAB:
      return "matlab";

    case SU_DUMP_FORMAT_WAV:
      return "wav";

    case SU_DUMP_FORMAT_RAW:
      return "raw";

    default:
      return "unknown";
  }
}

SUBOOL
su_test_run(
    const su_test_entry_t *test_list,
    unsigned int test_count,
    unsigned int range_start,
    unsigned int range_end,
    const struct su_test_run_params *params)
{
  su_test_context_t ctx = su_test_context_INITIALIZER;
  unsigned int i;
  unsigned int count = 0;
  unsigned int success = 0;
  time_t now;

  if (range_end >= test_count)
    range_end = test_count - 1;

  time(&now);

  SU_INFO("Sigutils library unit test starting: %s", ctime(&now));
  SU_INFO("  Configured buffer size: %d elements\n", params->buffer_size);
  SU_INFO("  Dumping buffers: %s\n", params->dump_fmt ? "yes" : "no");
  SU_INFO("  Size of SUFLOAT: %d bits\n", sizeof(SUFLOAT) << 3);

  if (params->dump_fmt)
    SU_INFO(
        "  Dump format: %s\n",
        su_test_dump_format_to_string(params->dump_fmt));

  for (i = range_start; i <= range_end; ++i) {
    ctx.testno = i;
    ctx.entry = &test_list[i];
    ctx.params = params;

    if ((ctx.pool = su_sigbuf_pool_new(ctx.entry->name)) == NULL) {
      SU_ERROR("Failed to initialize pool\n");
      goto next_test;
    }

    if (params->dump_fmt
        && !su_sigbuf_pool_helper_ensure_directory(ctx.entry->name)) {
      SU_ERROR("Failed to ensure dump directory\n");
      goto next_test;
    }

    if ((test_list[i].cb)(&ctx))
      ++success;
    else {
      printf("[t:%3d] TEST FAILED\n", i);
    }

    if (params->dump_fmt) {
      if (!su_sigbuf_pool_dump(ctx.pool, params->dump_fmt)) {
        SU_ERROR("Failed to dump allocated buffers\n");
        goto next_test;
      }

      su_sigbuf_pool_debug(ctx.pool);
    }

    ++count;

next_test:
    su_test_context_reset(&ctx);
  }

  printf("%d tests run, %d failed\n", count, count - success);

  return count == success;
}
