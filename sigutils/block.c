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

#include <util.h>
#include <string.h>

#define SU_LOG_LEVEL "block"

#include "log.h"
#include "block.h"

static su_block_class_t *class_list;
static unsigned int      class_storage;
static unsigned int      class_count;

/****************************** su_stream API ********************************/
SU_CONSTRUCTOR(su_stream, SUSCOUNT size)
{
  int i = 0;

  memset(self, 0, sizeof(su_stream_t));

  SU_ALLOCATE_MANY_CATCH(
    self->buffer,
    size,
    SUCOMPLEX,
    return SU_FALSE);
    
  /* Populate uninitialized buffer with NaNs */
  for (i = 0; i < size; ++i)
    self->buffer[i] = nan("uninitialized");

  self->size  = size;
  self->ptr = 0;
  self->avail = 0;
  self->pos   = 0ull;

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

  memcpy(self->buffer + self->ptr, data, chunksz * sizeof (SUCOMPLEX));
  self->ptr += chunksz;

  /* Rollover only can happen here */
  if (self->ptr == self->size) {
    self->ptr = 0;

    /* Is there anything left to be written? */
    if (size > 0) {
      size -= chunksz;
      data += chunksz;

      memcpy(self->buffer + self->ptr, data, size * sizeof (SUCOMPLEX));
      self->ptr += size;
    }
  }
}

SU_GETTER(su_stream, su_off_t, tell)
{
  return self->pos - self->avail;
}

SU_GETTER(
  su_stream, 
  SUSCOUNT, 
  get_contiguous, 
  SUCOMPLEX **start, 
  SUSCOUNT size)
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

  memcpy(data, self->buffer + ptr, chunksz * sizeof (SUCOMPLEX));
  size -= chunksz;

  /* Is there anything left to read? */
  if (size > 0)
    memcpy(data + chunksz, self->buffer, size * sizeof (SUCOMPLEX));

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
  pthread_mutex_lock(&fc->acquire_lock);
}

SUPRIVATE void
su_flow_controller_leave(su_flow_controller_t *fc)
{
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
  su_flow_controller_notify_force(fc);
}

SUPRIVATE void
su_flow_controller_force_eos(su_flow_controller_t *fc)
{
  fc->eos = SU_TRUE;

  su_flow_controller_notify(fc);
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
        return SU_FLOW_CONTROLLER_INTERNAL_ERROR;
    }

    /* Wakeups may be triggered by a forced EOS condition */
    if (fc->eos)
      return SU_FLOW_CONTROLLER_END_OF_STREAM;
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

  if (block->privdata != NULL)
    block->classname->dtor(block->privdata);

  if (block->in != NULL)
    free(block->in);

  if (block->out != NULL) {
    for (i = 0; i < block->classname->out_size; ++i) {
      su_flow_controller_finalize(&block->out[i]);
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

  new->classname = class;

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
  if (!class->ctor(new, &new->privdata, ap)) {
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
        &new->out[i],
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
  if (id >= block->classname->in_size) {
    return NULL;
  }

  return block->in + id;
}

su_flow_controller_t *
su_block_get_flow_controller(const su_block_t *block, unsigned int id)
{
  if (id >= block->classname->out_size) {
    return NULL;
  }

  return block->out + id;
}

SUBOOL
su_block_force_eos(const su_block_t *block, unsigned int id)
{
  su_flow_controller_t *fc;

  if ((fc = su_block_get_flow_controller(block, id)) == NULL)
    return SU_FALSE;

  su_flow_controller_force_eos(fc);

  return SU_TRUE;
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
su_block_set_master_port(
    su_block_t *block,
    unsigned int port_id,
    const su_block_port_t *port)
{
  su_flow_controller_t *fc;

  if ((fc = su_block_get_flow_controller(block, port_id)) == NULL)
    return SU_FALSE;

  if (fc->kind != SU_FLOW_CONTROL_KIND_MASTER_SLAVE)
    return SU_FALSE;

  fc->master = port;

  return SU_TRUE;
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
        sink->classname->name,
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
        port->block->classname->name);
    return SU_FALSE;
  }

  if (portid >= block->classname->out_size) {
    SU_ERROR("Block `%s' has no output #%d\n", block->classname->name, portid);
    return SU_FALSE;
  }

  port->port_id = portid;
  port->fc      = block->out + portid;
  port->block   = block;

  su_flow_controller_add_consumer(port->fc);
  port->pos     = su_flow_controller_tell(port->fc);

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

    if (port->fc->eos) {
      /* EOS forced somewhere */
      su_flow_controller_leave(port->fc);
      return SU_BLOCK_PORT_READ_END_OF_STREAM;
    }

    /* ------8<----- ENTER CONCURRENT FLOW CONTROLLER ACCESS -----8<------ */
    port->reading = SU_TRUE;
    got = su_flow_controller_read_unsafe(port->fc, port, port->pos, obuf, size);
    port->reading = SU_FALSE;

    switch (got) {
      case SU_FLOW_CONTROLLER_DESYNC:
        port->pos = su_flow_controller_tell(port->fc);
        su_flow_controller_leave(port->fc);
        return SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC;

      case SU_FLOW_CONTROLLER_INTERNAL_ERROR:
      case SU_FLOW_CONTROLLER_END_OF_STREAM:
        su_flow_controller_leave(port->fc);
        return SU_BLOCK_PORT_READ_END_OF_STREAM;

      case SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED:
        /*
         * Stream exhausted, and flow controller allowed this thread
         * to call acquire. Since this call is protected, the block
         * implementation doesn't have to worry about threads.
         */
        if ((acquired = port->block->classname->acquire(
            port->block->privdata,
            su_flow_controller_get_stream(port->block->out),
            port->port_id,
            port->block->in)) == -1) {
          /* Acquire error */
          SU_ERROR("%s: acquire failed\n", port->block->classname->name);
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
        break;

      default:
        if (got < 0) {
          SU_ERROR("Unexpected return value %d\n", got);
          su_flow_controller_leave(port->fc);
          return SU_BLOCK_PORT_READ_END_OF_STREAM;
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

  su_flow_controller_enter(port->fc);

  port->pos = su_flow_controller_tell(port->fc);

  su_flow_controller_leave(port->fc);

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

