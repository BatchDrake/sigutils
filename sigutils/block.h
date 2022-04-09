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

#ifndef _SIGUTILS_BLOCK_H
#define _SIGUTILS_BLOCK_H

#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <util.h>

#include "defs.h"
#include "property.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SU_BLOCK_STREAM_BUFFER_SIZE 4096

#define SU_BLOCK_PORT_READ_END_OF_STREAM 0
#define SU_BLOCK_PORT_READ_ERROR_NOT_INITIALIZED -1
#define SU_BLOCK_PORT_READ_ERROR_ACQUIRE -2
#define SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC -3

#define SU_FLOW_CONTROLLER_ACQUIRE_ALLOWED 0
#define SU_FLOW_CONTROLLER_DESYNC -1
#define SU_FLOW_CONTROLLER_END_OF_STREAM -2
#define SU_FLOW_CONTROLLER_INTERNAL_ERROR -3

typedef uint64_t su_off_t;

struct sigutils_stream {
  SUCOMPLEX *buffer;
  unsigned int size;  /* Stream size */
  unsigned int ptr;   /* Buffer pointer */
  unsigned int avail; /* Samples available for reading */

  su_off_t pos; /* Stream position */
};

typedef struct sigutils_stream su_stream_t;

#define su_stream_INITIALIZER \
  {                           \
    NULL,  /* buffer */       \
        0, /* size */         \
        0, /* ptr */          \
        0, /* avail */        \
        0  /* post */         \
  }

/* su_stream operations */
SU_CONSTRUCTOR(su_stream, SUSCOUNT size);
SU_DESTRUCTOR(su_stream);

SU_METHOD(su_stream, void, write, const SUCOMPLEX *data, SUSCOUNT size);
SU_METHOD(su_stream, SUSCOUNT, advance_contiguous, SUSCOUNT size);
SU_GETTER(su_stream,
          SUSCOUNT,
          get_contiguous,
          SUCOMPLEX **start,
          SUSCOUNT size);
SU_GETTER(su_stream, su_off_t, tell);
SU_GETTER(su_stream,
          SUSDIFF,
          read,
          su_off_t off,
          SUCOMPLEX *data,
          SUSCOUNT size);

/**************************** DEPRECATED API ********************************/
struct sigutils_block;

enum sigutils_flow_controller_kind {
  /*
   * Default flow control: this is like having no flow control whatsoever.
   * If a port reader is faster than some other, the slower one may lose
   * samples as the fastest will call acquire() earlier.
   */
  SU_FLOW_CONTROL_KIND_NONE = 0,

  /*
   * Barrier flow control: all port users must consume their stream buffers
   * before calling acquire()
   */
  SU_FLOW_CONTROL_KIND_BARRIER,

  /*
   * Master-slave flow control: only one port (the master) can trigger a call
   * to acquire(). This is useful if the master is the slowest port, or if
   * it's not critical that the slaves lose samples.
   */
  SU_FLOW_CONTROL_KIND_MASTER_SLAVE,
};

struct sigutils_block_port;

/*
 * Flow controllers ensure safe concurrent access to block output streams.
 * However, this model imposes a restriction: if non-null flow controller is
 * being used, read operation on the flow controller must be performed from
 * one and only one thread, otherwise deadlocks will occur. This happens
 * because after the end of the output stream is reached, the read operation
 * from the first port will sleep until the next port completes. However, since
 * the next port is in the same thread, the next read operation will never
 * take place.
 */
struct sigutils_flow_controller {
  enum sigutils_flow_controller_kind kind;
  SUBOOL eos;
  pthread_mutex_t acquire_lock;
  pthread_cond_t acquire_cond;
  su_stream_t output;     /* Output stream */
  unsigned int consumers; /* Number of ports plugged to this flow controller */
  unsigned int pending;   /* Number of ports waiting for new data */
  const struct sigutils_block_port *master; /* Master port */
};

typedef struct sigutils_flow_controller su_flow_controller_t;

/*
 * Even though flow controllers are thread-safe by definition, block ports
 * are not. Don't attempt to use the same block port in different threads.
 */
struct sigutils_block_port {
  su_off_t pos;                 /* Current reading position in this port */
  su_flow_controller_t *fc;     /* Flow controller */
  struct sigutils_block *block; /* Input block */
  unsigned int port_id;
  SUBOOL reading;
};

typedef struct sigutils_block_port su_block_port_t;

#define su_block_port_INITIALIZER \
  {                               \
    0, NULL, NULL, 0, SU_FALSE    \
  }

struct sigutils_block_class {
  const char *name;
  unsigned int in_size;
  unsigned int out_size;

  /* Generic constructor / destructor */
  SUBOOL (*ctor)(struct sigutils_block *block, void **privdata, va_list);
  void (*dtor)(void *privdata);

  /* This function gets called when more data is required */
  SUSDIFF (*acquire)(void *, su_stream_t *, unsigned int, su_block_port_t *);
};

typedef struct sigutils_block_class su_block_class_t;

struct sigutils_block {
  /* Block overall configuration */
  su_block_class_t *classname;
  su_property_set_t properties;
  void *privdata;

  /* Architectural properties */
  su_block_port_t *in;       /* Input ports */
  su_flow_controller_t *out; /* Output streams */
  SUSCOUNT decimation;       /* Block decimation */
};

typedef struct sigutils_block su_block_t;

/* su_block operations */
su_block_t *su_block_new(const char *, ...);

su_block_port_t *su_block_get_port(const su_block_t *, unsigned int);

su_stream_t *su_block_get_stream(const su_block_t *, unsigned int);

SUBOOL su_block_plug(su_block_t *source,
                     unsigned int out_id,
                     unsigned int in_id,
                     su_block_t *sink);

su_property_t *su_block_lookup_property(const su_block_t *block,
                                        const char *name);

void *su_block_get_property_ref(const su_block_t *block,
                                su_property_type_t type,
                                const char *name);

SUBOOL su_block_set_property_ref(su_block_t *block,
                                 su_property_type_t type,
                                 const char *name,
                                 void *ptr);

void su_block_destroy(su_block_t *);

/* su_block_port operations */
SUBOOL su_block_port_plug(
    su_block_port_t *port,
    struct sigutils_block *block,
    unsigned int portid); /* Position initialized with current stream pos */

SUSDIFF
su_block_port_read(su_block_port_t *port, SUCOMPLEX *obuf, SUSCOUNT size);

/* Sometimes, a port connection may go out of sync. This fixes it */
SUBOOL su_block_port_resync(su_block_port_t *port);

SUBOOL su_block_port_is_plugged(const su_block_port_t *port);

void su_block_port_unplug(su_block_port_t *port);

SUBOOL su_block_force_eos(const su_block_t *block, unsigned int id);

SUBOOL su_block_set_flow_controller(su_block_t *block,
                                    unsigned int port_id,
                                    enum sigutils_flow_controller_kind kind);

SUBOOL su_block_set_master_port(su_block_t *block,
                                unsigned int port_id,
                                const su_block_port_t *port);

/* su_block_class operations */
SUBOOL su_block_class_register(struct sigutils_block_class *classname);

su_block_class_t *su_block_class_lookup(const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_BLOCK_H */
