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

#include <math.h>
#include <string.h>
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

SUPRIVATE void
su_test_context_reset(su_test_context_t *ctx)
{
  memset(ctx, 0, sizeof (su_test_context_t));

  ctx->time_units = SU_TIME_UNITS_UNDEFINED;
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
  SUFLOAT size_inv = 1. / size;
  SUFLOAT result = 0.;
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

  if (fprintf(fp, "\n];\n", arrname) < 0)
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

  if (fprintf(fp, "\n];\n", arrname) < 0)
      goto fail;

  fclose(fp);

  return SU_TRUE;

fail:
  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}

SUBOOL
su_test_run(
    const su_test_cb_t *test_list,
    unsigned int test_count,
    unsigned int range_start,
    unsigned int range_end,
    SUBOOL save)
{
  su_test_context_t ctx = su_test_context_INITIALIZER;
  unsigned int i;
  unsigned int count = 0;
  unsigned int success = 0;

  ctx.dump_results = save;

  if (range_end >= test_count)
    range_end = test_count - 1;

  for (i = range_start; i <= range_end; ++i) {
    ctx.testno = i;
    if ((test_list[i])(&ctx))
      ++success;
    else {
      printf("[t:%3d] TEST FAILED\n", i);
    }

    ++count;

    su_test_context_reset(&ctx);
  }

  printf("%d tests run, %d failed\n", count, count - success);

  return count == success;
}
