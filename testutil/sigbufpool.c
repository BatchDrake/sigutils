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
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include "test.h"

SUPRIVATE void
su_sigbuf_destroy(su_sigbuf_t *sbuf)
{
  if (sbuf->name != NULL)
    free(sbuf->name);

  if (sbuf->buffer != NULL)
    free(sbuf->buffer);

  free(sbuf);
}

SUPRIVATE su_sigbuf_t *
su_sigbuf_new(const char *name, size_t size, SUBOOL is_complex)
{
  su_sigbuf_t *new = NULL;

  if ((new = calloc(1, sizeof (su_sigbuf_t))) == NULL)
    goto fail;

  if ((new->buffer = calloc(
      size,
      is_complex ? sizeof (SUCOMPLEX) : sizeof (SUFLOAT))) == NULL)
    goto fail;

  if ((new->name = strdup(name)) == NULL)
    goto fail;

  new->is_complex = is_complex;
  new->size = size;

  return new;

fail:
  if (new != NULL)
    su_sigbuf_destroy(new);

  return NULL;
}

void
su_sigbuf_pool_destroy(su_sigbuf_pool_t *pool)
{
  unsigned int i;
  su_sigbuf_t *this;

  FOR_EACH_PTR(this, i, pool->sigbuf)
    su_sigbuf_destroy(this);

  if (pool->sigbuf_list != NULL)
    free(pool->sigbuf_list);

  if (pool->name != NULL)
    free(pool->name);

  free(pool);
}

su_sigbuf_pool_t *
su_sigbuf_pool_new(const char *name)
{
  su_sigbuf_pool_t *new = NULL;

  if ((new = calloc(1, sizeof (su_sigbuf_pool_t))) == NULL)
    goto fail;

  if ((new->name = strdup(name)) == NULL)
    goto fail;

  return new;

fail:
  if (new != NULL)
    su_sigbuf_pool_destroy(new);

  return NULL;
}

SUPRIVATE su_sigbuf_t *
su_sigbuf_pool_lookup(su_sigbuf_pool_t *pool, const char *name)
{
  unsigned int i;
  su_sigbuf_t *this;

  FOR_EACH_PTR(this, i, pool->sigbuf)
    if (strcmp(this->name, name) == 0)
      return this;

  return NULL;
}

SUPRIVATE void *
su_sigbuf_pool_get(
    su_sigbuf_pool_t *pool,
    const char *name,
    size_t size,
    SUBOOL is_complex)
{
  su_sigbuf_t *this;
  su_sigbuf_t *new = NULL;

  if ((this = su_sigbuf_pool_lookup(pool, name)) != NULL) {
    /* Buffer already exists, check metadata */

    if (this->is_complex != is_complex) {
      SU_ERROR("Buffer already allocated, mismatching complexity\n");
      goto fail;
    }

    if (this->size != size) {
      SU_ERROR("Buffer already allocated, mismatching size\n");
      goto fail;
    }

    return this->buffer;
  }

  if ((new = su_sigbuf_new(name, size, is_complex)) == NULL)
    goto fail;

  if (PTR_LIST_APPEND_CHECK(pool->sigbuf, new) == -1)
    goto fail;

  return new->buffer;

fail:
  if (new != NULL)
    su_sigbuf_destroy(new);

  return NULL;
}

SUFLOAT *
su_sigbuf_pool_get_float(su_sigbuf_pool_t *pool, const char *name, size_t size)
{
  return (SUFLOAT *) su_sigbuf_pool_get(pool, name, size, SU_FALSE);
}

SUCOMPLEX *
su_sigbuf_pool_get_complex(su_sigbuf_pool_t *pool, const char *name, size_t size)
{
  return (SUCOMPLEX *) su_sigbuf_pool_get(pool, name, size, SU_TRUE);
}

SUBOOL
su_sigbuf_pool_helper_ensure_directory(const char *name)
{
  struct stat sbuf;

  if (stat(name, &sbuf) != -1) {
    if (!S_ISDIR(sbuf.st_mode)) {
      SU_ERROR("File %s exists, but is not a directory\n", name);
      return SU_FALSE;
    }
  } else if (mkdir(name, 0755) == -1) {
    SU_ERROR("Failed to create `%s': %s\n", name, strerror(errno));
    return SU_FALSE;
  }

  return SU_TRUE;
}

SUBOOL
su_sigbuf_pool_helper_dump(
    const void *data,
    size_t size,
    SUBOOL is_complex,
    const char *directory,
    const char *name)
{

  char *filename = NULL;
  SUBOOL result;

  filename = strbuild("%s/%s.m", directory, name);
  if (filename == NULL) {
    SU_ERROR("Memory error while building filename\n");
    goto fail;
  }

  if (is_complex)
    result = su_test_complex_buffer_dump_matlab(
        (const SUCOMPLEX *) data,
        size,
        filename,
        name);
  else
    result = su_test_buffer_dump_matlab(
        (const SUFLOAT *) data,
        size,
        filename,
        name);

  free(filename);

  return result;

fail:
  if (filename != NULL)
    free(filename);

  return SU_FALSE;
}

#define SU_SYSCALL_ASSERT(expr)                                 \
  if ((expr) < 0) {                                             \
    SU_ERROR(                                                   \
          "Operation `%s' failed (negative value returned)\n",  \
          STRINGIFY(expr));                                     \
    goto fail;                                                  \
  }

void
su_sigbuf_pool_debug(const su_sigbuf_pool_t *pool)
{
  su_sigbuf_t *this;
  unsigned int i;
  size_t allocation = 0;
  size_t total = 0;

  SU_INFO("Pool `%s' status:\n", pool->name);
  SU_INFO(" ID  Buf name   Type      Size   Allocation size\n");
  SU_INFO("------------------------------------------------\n");
  FOR_EACH_PTR(this, i, pool->sigbuf) {
    allocation = this->size *
        (this->is_complex ? sizeof (SUCOMPLEX) : sizeof (SUFLOAT));
    SU_INFO("[%2d] %-10s %-7s %8d %8d bytes\n",
            i,
            this->name,
            this->is_complex ? "COMPLEX" : "FLOAT",
            this->size,
            allocation);

    total += allocation;
  }
  SU_INFO("------------------------------------------------\n");
  SU_INFO("Total: %d bytes\n", total);
}

SUBOOL
su_sigbuf_pool_dump(su_sigbuf_pool_t *pool)
{
  su_sigbuf_t *this;
  unsigned int i;
  char *filename = NULL;
  FILE *fp = NULL;
  time_t now;

  if (!su_sigbuf_pool_helper_ensure_directory(pool->name))
    goto fail;

  if ((filename = strbuild("%s.m", pool->name)) == NULL) {
    SU_ERROR("Failed to build main script file name\n");
    goto fail;
  }

  if ((fp = fopen(filename, "w")) == NULL) {
    SU_ERROR("Cannot create main script `%s': %s\n", filename, strerror(errno));
    goto fail;
  }

  free(filename);
  filename = NULL;

  time(&now);

  SU_SYSCALL_ASSERT(
      fprintf(
          fp,
          "%% Autogenerated MATLAB script for sigbuf pool `%s'\n",
          pool->name));
  SU_SYSCALL_ASSERT(
      fprintf(
          fp,
          "%% File generated on %s",
          ctime(&now)));

  FOR_EACH_PTR(this, i, pool->sigbuf) {
    if (!su_sigbuf_pool_helper_dump(
        this->buffer,
        this->size,
        this->is_complex,
        pool->name,
        this->name))
      goto fail;

    SU_SYSCALL_ASSERT(
        fprintf(
            fp,
            "%% %s: %s buffer, %d elements\n",
            this->name,
            this->is_complex ? "complex" : "float",
            this->size));
    SU_SYSCALL_ASSERT(
        fprintf(
            fp,
            "source('%s/%s.m');\n\n",
            pool->name,
            this->name));
  }

  fclose(fp);
  fp = NULL;

  return SU_TRUE;

fail:
  if (filename != NULL)
    free(filename);

  if (fp != NULL)
    fclose(fp);

  return SU_FALSE;
}
