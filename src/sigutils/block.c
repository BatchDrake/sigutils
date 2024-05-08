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

#include <string.h>
#include <sigutils/util/util.h>

#define SU_LOG_LEVEL "block"

#include <sigutils/block.h>
#include <sigutils/log.h>

/****************************** su_stream API ********************************/
SU_CONSTRUCTOR(su_stream, SUSCOUNT size)
{
  SUSCOUNT i = 0;

  memset(self, 0, sizeof(su_stream_t));

  SU_ALLOCATE_MANY_CATCH(self->buffer, size, SUCOMPLEX, return SU_FALSE);

  /* Populate uninitialized buffer with NaNs */
  for (i = 0; i < size; ++i)
    self->buffer[i] = nan("uninitialized");

  self->size = size;
  self->ptr = 0;
  self->avail = 0;
  self->pos = 0ull;

  return SU_TRUE;
}

SU_DESTRUCTOR(su_stream)
{
  if (self->buffer != NULL)
    free(self->buffer);
}

SU_METHOD(su_stream, void, write, const SUCOMPLEX *data, SUSCOUNT size)
{
  SUSCOUNT skip = 0;
  SUSCOUNT chunksz;

  /*
   * We increment this always. Current reading position is
   *  stream->pos - stream->avail
   */
  self->pos += size;

  if (size > self->size) {
    SU_WARNING("write will overflow stream, keeping latest samples\n");

    skip = size - self->size;
    data += skip;
    size -= skip;
  }

  if ((chunksz = self->size - self->ptr) > size)
    chunksz = size;

  /* This needs to be updated only once */
  if (self->avail < self->size)
    self->avail += chunksz;

  memcpy(self->buffer + self->ptr, data, chunksz * sizeof(SUCOMPLEX));
  self->ptr += chunksz;

  /* Rollover only can happen here */
  if (self->ptr == self->size) {
    self->ptr = 0;

    /* Is there anything left to be written? */
    if (size > 0) {
      size -= chunksz;
      data += chunksz;

      memcpy(self->buffer + self->ptr, data, size * sizeof(SUCOMPLEX));
      self->ptr += size;
    }
  }
}

SU_GETTER(su_stream, su_off_t, tell)
{
  return self->pos - self->avail;
}

SU_GETTER(su_stream, SUSCOUNT, get_contiguous, SUCOMPLEX **start, SUSCOUNT size)
{
  SUSCOUNT avail = self->size - self->ptr;

  if (size > avail) {
    size = avail;
  }

  *start = self->buffer + self->ptr;

  return size;
}

SU_METHOD(su_stream, SUSCOUNT, advance_contiguous, SUSCOUNT size)
{
  SUSCOUNT avail = self->size - self->ptr;

  if (size > avail) {
    size = avail;
  }

  self->pos += size;
  self->ptr += size;
  if (self->avail < self->size) {
    self->avail += size;
  }

  /* Rollover */
  if (self->ptr == self->size) {
    self->ptr = 0;
  }

  return size;
}

SU_GETTER(
    su_stream,
    SUSDIFF,
    read,
    su_off_t off,
    SUCOMPLEX *data,
    SUSCOUNT size)
{
  SUSCOUNT avail;
  su_off_t readpos = su_stream_tell(self);
  SUSCOUNT reloff;
  SUSCOUNT chunksz;
  SUSDIFF ptr;

  /* Slow reader */
  if (off < readpos)
    return -1;

  /* Greedy reader */
  if (off >= self->pos)
    return 0;

  reloff = off - readpos;

  /* Compute how many samples are available from here */
  avail = self->avail - reloff;
  if (avail < size) {
    size = avail;
  }

  /* Compute position in the stream buffer to read from */
  if ((ptr = self->ptr - avail) < 0)
    ptr += self->size;

  /* Adjust in case reloff causes ptr to rollover */
  if (ptr > self->size)
    ptr = ptr - self->size;

  if (ptr + size > self->size)
    chunksz = self->size - ptr;
  else
    chunksz = size;

  memcpy(data, self->buffer + ptr, chunksz * sizeof(SUCOMPLEX));
  size -= chunksz;

  /* Is there anything left to read? */
  if (size > 0)
    memcpy(data + chunksz, self->buffer, size * sizeof(SUCOMPLEX));

  return chunksz + size;
}
