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
su_sigbuf_new(const char *name, SUSCOUNT size, SUBOOL is_complex)
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
su_sigbuf_set_fs(su_sigbuf_t *sbuf, SUSCOUNT fs)
{
  sbuf->fs = fs;
}

SUSCOUNT
su_sigbuf_get_fs(const su_sigbuf_t *sbuf)
{
  return sbuf->fs;
}

SUBOOL
su_sigbuf_resize(su_sigbuf_t *sbuf, SUSCOUNT size)
{
  void *new_buffer = NULL;

  if (size > 0) {
    if ((new_buffer = realloc(
        sbuf->buffer,
        size
          * (sbuf->is_complex ? sizeof (SUCOMPLEX) : sizeof (SUFLOAT)))) == NULL)
      return SU_FALSE;

    sbuf->buffer = new_buffer;
    sbuf->size = size;
  }

  return SU_TRUE;
}

void
su_sigbuf_pool_destroy(su_sigbuf_pool_t *pool)
{
  unsigned int i;
  su_sigbuf_t *this;

  FOR_EACH_PTR(this, pool, sigbuf)
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

  new->fs = SU_SIGBUF_SAMPLING_FREQUENCY_DEFAULT;

  return new;

fail:
  if (new != NULL)
    su_sigbuf_pool_destroy(new);

  return NULL;
}

su_sigbuf_t *
su_sigbuf_pool_lookup(su_sigbuf_pool_t *pool, const char *name)
{
  unsigned int i;
  su_sigbuf_t *this;

  FOR_EACH_PTR(this, pool, sigbuf)
    if (strcmp(this->name, name) == 0)
      return this;

  return NULL;
}

SUPRIVATE void *
su_sigbuf_pool_get(
    su_sigbuf_pool_t *pool,
    const char *name,
    SUSCOUNT size,
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

  su_sigbuf_set_fs(new, pool->fs);

  if (PTR_LIST_APPEND_CHECK(pool->sigbuf, new) == -1)
    goto fail;

  return new->buffer;

fail:
  if (new != NULL)
    su_sigbuf_destroy(new);

  return NULL;
}

void
su_sigbuf_pool_set_fs(su_sigbuf_pool_t *pool, SUSCOUNT fs)
{
  pool->fs = fs;
}

SUSCOUNT
su_sigbuf_pool_get_fs(const su_sigbuf_pool_t *pool)
{
  return pool->fs;
}

SUFLOAT *
su_sigbuf_pool_get_float(su_sigbuf_pool_t *pool, const char *name, SUSCOUNT size)
{
  return (SUFLOAT *) su_sigbuf_pool_get(pool, name, size, SU_FALSE);
}

SUCOMPLEX *
su_sigbuf_pool_get_complex(su_sigbuf_pool_t *pool, const char *name, SUSCOUNT size)
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
  FOR_EACH_PTR(this, pool, sigbuf) {
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
su_sigbuf_pool_dump(const su_sigbuf_pool_t *pool, enum sigutils_dump_format f)
{
  if (!su_sigbuf_pool_helper_ensure_directory(pool->name))
    return SU_FALSE;

  switch (f) {
    case SU_DUMP_FORMAT_NONE:
      return SU_TRUE;

    case SU_DUMP_FORMAT_MATLAB:
      return su_sigbuf_pool_dump_matlab(pool);

    case SU_DUMP_FORMAT_WAV:
      return su_sigbuf_pool_dump_wav(pool);

    case SU_DUMP_FORMAT_RAW:
      return su_sigbuf_pool_dump_raw(pool);
  }

  return SU_TRUE;
}

