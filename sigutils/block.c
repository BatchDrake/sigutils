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

#include <util.h>
#include <string.h>
#include "block.h"

static su_block_class_t *class_list;
static unsigned int      class_storage;
static unsigned int      class_count;

/****************************** su_stream API ********************************/
SUBOOL
su_stream_init(su_stream_t *stream, size_t size)
{
  SUCOMPLEX *buffer = NULL;

  if ((buffer = malloc(size * sizeof (SUCOMPLEX)))) {
    SU_ERROR("buffer allocation failed\n");
    return SU_FALSE;
  }

  stream->buffer = buffer;
  stream->size  = size;
  stream->ptr = 0;
  stream->avail = 0;
  stream->pos   = 0ull;

  return SU_TRUE;
}

void
su_stream_finalize(su_stream_t *stream)
{
  if (stream->buffer != NULL)
    free(stream->buffer);
}

void
su_stream_write (su_stream_t *stream, const SUCOMPLEX *data, size_t size)
{
  size_t skip = 0;
  size_t chunksz;

  /*
   * We increment this always. Current reading position is
   *  stream->pos - stream->avail
   */
  stream->pos += size;

  if (size > stream->size) {
    SU_WARNING("write will overflow stream, keeping latest samples\n");

    skip = size - stream->size;
    data += skip;
    size -= skip;
  }

  if ((chunksz = stream->size - stream->ptr) > size)
    chunksz = size;

  /* This needs to be updated only once */
  if (stream->avail < stream->size)
    stream->avail += chunksz;

  memcpy(stream->buffer + stream->ptr, data, chunksz * sizeof (SUCOMPLEX));
  stream->ptr += chunksz;

  /* Rollover only can happen here */
  if (stream->ptr == stream->size) {
    stream->ptr = 0;

    /* Is there anything left to be written? */
    if (size > 0) {
      size -= chunksz;
      data += chunksz;

      memcpy(stream->buffer + stream->ptr, data, size * sizeof (SUCOMPLEX));
      stream->ptr += size;
    }
  }
}

su_off_t
su_stream_tell(const su_stream_t *stream)
{
  return stream->pos - stream->avail;
}


size_t
su_stream_get_contiguous(
    const su_stream_t *stream,
    SUCOMPLEX **start,
    size_t size)
{
  size_t avail = stream->size - stream->ptr;

  if (size > avail) {
    size = avail;
  }

  *start = stream->buffer + stream->ptr;

  return size;
}

size_t
su_stream_advance_contiguous(
    su_stream_t *stream,
    size_t size)
{
  size_t avail = stream->size - stream->ptr;

  if (size > avail) {
    size = avail;
  }

  stream->pos += size;
  stream->ptr += size;
  if (stream->avail < stream->size) {
    stream->avail += size;
  }

  /* Rollover */
  if (stream->ptr == stream->size) {
    stream->ptr = 0;
  }

  return size;
}

ssize_t
su_stream_read(su_stream_t *stream, su_off_t off, SUCOMPLEX *data, size_t size)
{
  size_t avail;
  su_off_t readpos = su_stream_tell(stream);
  size_t reloff;
  size_t chunksz;
  size_t ptr;

  /* Slow reader */
  if (off < readpos)
    return -1;

  /* Greedy reader */
  if (off >= stream->pos)
    return 0;

  reloff = off - readpos;

  /* Compute how many samples are available from here */
  avail = stream->avail - reloff;
  if (avail < size) {
    size = avail;
  }

  /* Compute position in the stream buffer to read from */
  ptr = stream->ptr + reloff;
  if (ptr + size > stream->size)
    chunksz = stream->size - ptr;
  else
    chunksz = size;

  memcpy(data, stream->buffer + ptr, chunksz * sizeof (SUCOMPLEX));
  size -= chunksz;

  /* Is there anything left to read? */
  if (size > 0) {
    data += chunksz;

    memcpy(data, stream->buffer + ptr + chunksz, size * sizeof (SUCOMPLEX));
  }

  return chunksz + size;
}

/*************************** su_block_class API ******************************/
su_block_class_t *
su_block_class_lookup(const char *name)
{
  unsigned int i;

  for (i = 0; i < class_count; ++i) {
    if (strcmp(class_list[i].name, name) == 0)
      return class_list + i;
  }

  return NULL;
}

SUBOOL
su_block_class_register(struct sigutils_block_class *class)
{
  su_block_class_t *tmp = NULL;
  unsigned int new_storage;

  if (su_block_class_lookup(class->name) != NULL) {
    SU_ERROR("block class `%s' already registered\n", class->name);
    return SU_FALSE;
  }

  if (class_count + 1 > class_storage) {
    if (class_storage == 0)
      new_storage = 1;
    else
      new_storage <<= 1;

    if ((tmp = realloc(
        class_list,
        new_storage * sizeof (su_block_class_t))) == NULL) {
      SU_ERROR("realloc() failed\n");
      return SU_FALSE;
    }

    class_list = tmp;
    class_storage = new_storage;
  }

  memcpy(class_list + class_count++, class, sizeof (su_block_class_t));

  return SU_TRUE;
}

/****************************** su_block API *********************************/
void
su_block_destroy(su_block_t *block)
{
  unsigned int i;

  if (block->private != NULL)
    block->class->dtor(block->private);

  if (block->in != NULL)
    free(block->in);

  if (block->out != NULL) {
    for (i = 0; i < block->class->out_size; ++i) {
      su_stream_finalize(block->out + i);
    }

    free(block->out);
  }

  free(block);
}

su_block_t *
su_block_new(const char *class_name, ...)
{
  va_list ap;
  su_block_t *new = NULL;
  su_block_t *result = NULL;
  su_block_class_t *class;
  unsigned int i;

  va_start(ap, class_name);

  if ((class = su_block_class_lookup(class_name)) == NULL) {
    SU_ERROR("No block class `%s' found\n", class_name);
    goto done;
  }

  if ((new = calloc(1, sizeof(su_block_t))) == NULL) {
    SU_ERROR("Cannot allocate block\n");
    goto done;
  }

  if (class->in_size > 0) {
    if ((new->in = calloc(class->in_size, sizeof(su_block_port_t))) == NULL) {
      SU_ERROR("Cannot allocate block input ports\n");
      goto done;
    }
  }

  if (class->out_size > 0) {
    if ((new->out = calloc(class->out_size, sizeof(su_stream_t))) == NULL) {
      SU_ERROR("Cannot allocate output streams\n");
      goto done;
    }
  }

  /* Initialize all outputs */
  for (i = 0; i < class->out_size; ++i)
    if (!su_stream_init(new->out, SU_BLOCK_STREAM_BUFFER_SIZE)) {
      SU_ERROR("Cannot allocate memory for block output #%d\n", i + 1);
      goto done;
    }

  /* Initialize object */
  new->class = class;
  if (!class->ctor(&new->private, ap)) {
    SU_ERROR("Call to `%s' constructor failed\n", class_name);
    goto done;
  }

  result = new;

done:
  if (result == NULL && new != NULL)
    su_block_destroy(new);

  va_end(ap);

  return result;
}

/************************** su_block_port API ********************************/
SUBOOL
su_block_port_is_plugged(const su_block_port_t *port)
{
  return port->block != NULL;
}

SUBOOL
su_block_port_plug(su_block_port_t *port,
    struct sigutils_block *block,
    unsigned int portid)
{
  if (su_block_port_is_plugged(port)) {
    SU_ERROR(
        "Port already plugged to block `%s'\n",
        port->block->class->name);
    return SU_FALSE;
  }

  if (portid >= block->class->out_size) {
    SU_ERROR("Block `%s' has no output #%d\n", block->class->name, portid);
    return SU_FALSE;
  }

  port->stream = block->out + portid;
  port->block  = block;
  port->pos    = su_stream_tell(port->stream);

  return SU_TRUE;
}

ssize_t
su_block_port_read(su_block_port_t *port, SUCOMPLEX *obuf, size_t size)
{
  ssize_t got = 0;
  ssize_t acquired = 0;

  if (!su_block_port_is_plugged(port)) {
    SU_ERROR("Port not plugged\n");
    return SU_BLOCK_PORT_READ_ERROR_NOT_INITIALIZED;
  }

  do {
    if ((got = su_stream_read(port->stream, port->pos, obuf, size)) == -1) {
      SU_ERROR("Port read failed (port desync)\n");
      return SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC;
    } else if (got == 0) {
      /* Stream exhausted, acquire more data */
      if ((acquired = port->block->class->acquire(
          port->block->private,
          port->block->out,
          port->block->in)) == -1) {
        /* Acquire error */
        SU_ERROR("%s: acquire failed\n", port->block->class->name);
        return SU_BLOCK_PORT_READ_ERROR_ACQUIRE;
      } else if (acquired == 0) {
        /* Stream closed */
        return SU_BLOCK_PORT_READ_END_OF_STREAM;
      }
    }
  } while (got == 0);

  port->pos += got;

  return got;
}

SUBOOL
su_block_port_resync(su_block_port_t *port)
{
  if (!su_block_port_is_plugged(port)) {
    SU_ERROR("Port not plugged\n");
    return SU_FALSE;
  }

  port->pos = su_stream_tell(port->stream);
}

void
su_block_port_unplug(su_block_port_t *port)
{
  port->block = NULL;
  port->stream = NULL;
  port->pos = 0;
}

