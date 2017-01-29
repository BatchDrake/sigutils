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
su_stream_init(su_stream_t *stream, SUSCOUNT size)
{
  SUCOMPLEX *buffer = NULL;
  int i = 0;

  if ((buffer = malloc(size * sizeof (SUCOMPLEX))) == NULL) {
    SU_ERROR("buffer allocation failed\n");
    return SU_FALSE;
  }

  /* Populate uninitialized buffer with NaNs */
  for (i = 0; i < size; ++i)
    buffer[i] = nan("uninitialized");

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
su_stream_write(su_stream_t *stream, const SUCOMPLEX *data, SUSCOUNT size)
{
  SUSCOUNT skip = 0;
  SUSCOUNT chunksz;

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


SUSCOUNT
su_stream_get_contiguous(
    const su_stream_t *stream,
    SUCOMPLEX **start,
    SUSCOUNT size)
{
  SUSCOUNT avail = stream->size - stream->ptr;

  if (size > avail) {
    size = avail;
  }

  *start = stream->buffer + stream->ptr;

  return size;
}

SUSCOUNT
su_stream_advance_contiguous(
    su_stream_t *stream,
    SUSCOUNT size)
{
  SUSCOUNT avail = stream->size - stream->ptr;

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

SUSDIFF
su_stream_read(const su_stream_t *stream, su_off_t off, SUCOMPLEX *data, SUSCOUNT size)
{
  SUSCOUNT avail;
  su_off_t readpos = su_stream_tell(stream);
  SUSCOUNT reloff;
  SUSCOUNT chunksz;
  SUSDIFF ptr;

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
  if ((ptr = stream->ptr - avail) < 0)
    ptr += stream->size;

  /* Adjust in case reloff causes ptr to rollover */
  if (ptr > stream->size)
    ptr = ptr - stream->size;

  if (ptr + size > stream->size)
    chunksz = stream->size - ptr;
  else
    chunksz = size;

  memcpy(data, stream->buffer + ptr, chunksz * sizeof (SUCOMPLEX));
  size -= chunksz;

  /* Is there anything left to read? */
  if (size > 0)
    memcpy(data + chunksz, stream->buffer, size * sizeof (SUCOMPLEX));

  return chunksz + size;
}

/************************* su_flow_controller API ****************************/
void
su_flow_controller_finalize(su_flow_controller_t *fc)
{
  su_stream_finalize(&fc->output);
  pthread_mutex_destroy(&fc->acquire_lock);
  pthread_cond_destroy(&fc->acquire_cond);
}

SUBOOL
su_flow_controller_init(
    su_flow_controller_t *fc,
    enum sigutils_flow_controller_kind kind,
    SUSCOUNT size)
{
  SUBOOL result = SU_FALSE;

  memset(fc, 0, sizeof (su_flow_controller_t));

  if (pthread_mutex_init(&fc->acquire_lock, NULL) == -1)
    goto done;

  if (pthread_cond_init(&fc->acquire_cond, NULL) == -1)
    goto done;

  if (!su_stream_init(&fc->output, size))
    goto done;

  fc->kind = kind;
  fc->consumers = 0;
  fc->pending = 0;

  result = SU_TRUE;

done:
  if (!result)
    su_flow_controller_finalize(fc);

  return result;
}

SUPRIVATE void
su_flow_controller_enter(su_flow_controller_t *fc)
{
  if (fc->consumers > 1)
    pthread_mutex_lock(&fc->acquire_lock);
}

SUPRIVATE void
su_flow_controller_leave(su_flow_controller_t *fc)
{
  if (fc->consumers > 1)
    pthread_mutex_unlock(&fc->acquire_lock);
}

SUPRIVATE void
su_flow_controller_notify_force(su_flow_controller_t *fc)
{
  pthread_cond_broadcast(&fc->acquire_cond);
}

SUPRIVATE void
su_flow_controller_notify(su_flow_controller_t *fc)
{
  if (fc->consumers > 1)
    su_flow_controller_notify_force(fc);
}

SUPRIVATE su_off_t
su_flow_controller_tell(const su_flow_controller_t *fc)
{
  return su_stream_tell(&fc->output);
}

SUPRIVATE su_stream_t *
su_flow_controller_get_stream(su_flow_controller_t *fc)
{
  return &fc->output;
}

/* TODO: make these functions thread safe */
SUPRIVATE void
su_flow_controller_add_consumer(su_flow_controller_t *fc)
{
  ++fc->consumers;
}

SUPRIVATE void
su_flow_controller_remove_consumer(su_flow_controller_t *fc, SUBOOL pend)
{
  --fc->consumers;

  if (fc->kind == SU_FLOW_CONTROL_KIND_BARRIER) {
    if (pend)
      --fc->pending;
    else if (fc->consumers > 0 && fc->consumers == fc->pending) {
      /* Wake up all pending threads and try to read again */
      fc->pending = 0;
      su_flow_controller_notify_force(fc);
    }
  } else if (fc->kind == SU_FLOW_CONTROL_KIND_MASTER_SLAVE) {
    /* TODO: mark flow control as EOF if master is being unplugged */
  }
}

SUPRIVATE SUBOOL
su_flow_controller_set_kind(
    su_flow_controller_t *fc,
    enum sigutils_flow_controller_kind kind)
{
  /* Cannot set flow control twice */
  if (fc->kind != SU_FLOW_CONTROL_KIND_NONE)
    return SU_FALSE;

  fc->kind = kind;

  return SU_TRUE;
}

SUPRIVATE SUSDIFF
su_flow_controller_read_unsafe(
    su_flow_controller_t *fc,
    struct sigutils_block_port *reader,
    su_off_t off,
    SUCOMPLEX *data,
    SUSCOUNT size)
{
  SUSDIFF result;

  while ((result = su_stream_read(&fc->output, off, data, size)) == 0
      && fc->consumers > 1) {
    /*
     * We have reached the end of the stream. In the concurrent case,
     * we may need to wait to repeat the read operation on the stream
     */

    switch (fc->kind) {
      case SU_FLOW_CONTROL_KIND_NONE:
        return SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED;

      case SU_FLOW_CONTROL_KIND_BARRIER:
        if (++fc->pending < fc->consumers)
          /* Greedy reader. Wait for the last one */
          pthread_cond_wait(&fc->acquire_cond, &fc->acquire_lock);
        else {
          /* Slow reader. Let caller perform acquire() */
          fc->pending = 0; /* Reset pending counter */
          return SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED;
        }

        break;

      case SU_FLOW_CONTROL_KIND_MASTER_SLAVE:
        if (fc->master != reader)
          /* Slave must wait for master to read */
          pthread_cond_wait(&fc->acquire_cond, &fc->acquire_lock);
        else
          return SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED;
        break;

      default:
        SU_ERROR("Invalid flow controller kind\n");
        return -1;
    }
  }

  return result;
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
  unsigned int new_storage = 0;

  if (su_block_class_lookup(class->name) != NULL) {
    SU_ERROR("block class `%s' already registered\n", class->name);
    return SU_FALSE;
  }

  if (class_count + 1 > class_storage) {
    if (class_storage == 0)
      new_storage = 1;
    else
      new_storage = class_storage << 1;

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
  su_property_t *prop;

  if (block->private != NULL)
    block->class->dtor(block->private);

  if (block->in != NULL)
    free(block->in);

  if (block->out != NULL) {
    for (i = 0; i < block->class->out_size; ++i) {
      su_flow_controller_finalize(block->out + i);
    }

    free(block->out);
  }

  su_property_set_finalize(&block->properties);

  free(block);
}

su_property_t *
su_block_lookup_property(const su_block_t *block, const char *name)
{
  return su_property_set_lookup(&block->properties, name);
}

void *
su_block_get_property_ref(
    const su_block_t *block,
    su_property_type_t type,
    const char *name) {
  const su_property_t *prop;

  if ((prop = su_block_lookup_property(block, name)) == NULL)
    return NULL;

  if (type != SU_PROPERTY_TYPE_ANY && prop->type != type)
    return NULL;

  return prop->generic_ptr;
}

SUBOOL
su_block_set_property_ref(
    su_block_t *block,
    su_property_type_t type,
    const char *name,
    void *ptr)
{
  su_property_t *prop;

  if ((prop = su_property_set_assert_property(&block->properties, name, type))
      == NULL) {
    SU_ERROR("Failed to assert property `%s'\n", name);
    return SU_FALSE;
  }

  prop->generic_ptr = ptr;

  return SU_TRUE;
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

  new->class = class;

  if (class->in_size > 0) {
    if ((new->in = calloc(class->in_size, sizeof(su_block_port_t))) == NULL) {
      SU_ERROR("Cannot allocate block input ports\n");
      goto done;
    }
  }

  if (class->out_size > 0) {
    if ((new->out = calloc(class->out_size, sizeof(su_flow_controller_t)))
        == NULL) {
      SU_ERROR("Cannot allocate output streams\n");
      goto done;
    }
  }

  /* Set decimation to 1, this may be changed by block constructor */
  new->decimation = 1;

  /* Initialize object */
  if (!class->ctor(new, &new->private, ap)) {
    SU_ERROR("Call to `%s' constructor failed\n", class_name);
    goto done;
  }

  if (new->decimation < 1 || new->decimation > SU_BLOCK_STREAM_BUFFER_SIZE) {
    SU_ERROR("Block requested impossible decimation %d\n", new->decimation);
    goto done;
  }

  /* Initialize all outputs */
  for (i = 0; i < class->out_size; ++i)
    if (!su_flow_controller_init(
        new->out,
        SU_FLOW_CONTROL_KIND_NONE,
        SU_BLOCK_STREAM_BUFFER_SIZE / new->decimation)) {
      SU_ERROR("Cannot allocate memory for block output #%d\n", i + 1);
      goto done;
    }

  /* Initialize flow control */
  result = new;

done:
  if (result == NULL && new != NULL)
    su_block_destroy(new);

  va_end(ap);

  return result;
}

su_block_port_t *
su_block_get_port(const su_block_t *block, unsigned int id)
{
  if (id >= block->class->in_size) {
    return NULL;
  }

  return block->in + id;
}

su_flow_controller_t *
su_block_get_flow_controller(const su_block_t *block, unsigned int id)
{
  if (id >= block->class->out_size) {
    return NULL;
  }

  return block->out + id;
}

SUBOOL
su_block_set_flow_controller(
    su_block_t *block,
    unsigned int port_id,
    enum sigutils_flow_controller_kind kind)
{
  su_flow_controller_t *fc;

  if ((fc = su_block_get_flow_controller(block, port_id)) == NULL)
    return SU_FALSE;

  return su_flow_controller_set_kind(fc, kind);
}

SUBOOL
su_block_plug(
    su_block_t *source,
    unsigned int out_id,
    unsigned int in_id,
    su_block_t *sink)
{
  su_block_port_t *input;

  if ((input = su_block_get_port(sink, in_id)) == NULL) {
    SU_ERROR(
        "Block `%s' doesn't have input port #%d\n",
        sink->class->name,
        in_id);
    return SU_FALSE;
  }

  return su_block_port_plug(input, source, out_id);
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

  port->port_id = portid;
  port->fc      = block->out + portid;
  port->block   = block;
  port->pos     = su_flow_controller_tell(port->fc);

  su_flow_controller_add_consumer(port->fc);

  return SU_TRUE;
}

SUSDIFF
su_block_port_read(su_block_port_t *port, SUCOMPLEX *obuf, SUSCOUNT size)
{
  SUSDIFF got = 0;
  SUSDIFF acquired = 0;

  if (!su_block_port_is_plugged(port)) {
    SU_ERROR("Port not plugged\n");
    return SU_BLOCK_PORT_READ_ERROR_NOT_INITIALIZED;
  }

  do {
    su_flow_controller_enter(port->fc);

    /* ------8<----- ENTER CONCURRENT FLOW CONTROLLER ACCESS -----8<------ */
    port->reading = SU_TRUE;
    got = su_flow_controller_read_unsafe(port->fc, port, port->pos, obuf, size);
    port->reading = SU_FALSE;

    if (got == -1) {
      SU_WARNING("Port read failed (port desync)\n");

      su_flow_controller_leave(port->fc);
      return SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC;
    } else if (got == SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED) {
      /*
       * Stream exhausted, and flow controller allowed this thread
       * to call acquire. Since this call is protected, the block
       * implementation doesn't have to worry about threads.
       */
      if ((acquired = port->block->class->acquire(
          port->block->private,
          su_flow_controller_get_stream(port->block->out),
          port->port_id,
          port->block->in)) == -1) {
        /* Acquire error */
        SU_ERROR("%s: acquire failed\n", port->block->class->name);
        /* TODO: set error condition in flow control */
        su_flow_controller_leave(port->fc);
        return SU_BLOCK_PORT_READ_ERROR_ACQUIRE;
      } else if (acquired == 0) {
        /* Stream closed */
        /* TODO: set error condition in flow control */
        su_flow_controller_leave(port->fc);
        return SU_BLOCK_PORT_READ_END_OF_STREAM;
      } else {
        /* Acquire succeeded, wake up all threads */
        su_flow_controller_notify(port->fc);
      }
    }
    /* ------>8----- LEAVE CONCURRENT FLOW CONTROLLER ACCESS ----->8------ */

    su_flow_controller_leave(port->fc);

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

  port->pos = su_flow_controller_tell(port->fc);

  return SU_TRUE;
}

void
su_block_port_unplug(su_block_port_t *port)
{
  if (su_block_port_is_plugged(port)) {
    su_flow_controller_remove_consumer(port->fc, port->reading);
    port->block = NULL;
    port->fc = NULL;
    port->pos = 0;
    port->port_id = 0;
    port->reading = SU_FALSE;
  }
}

