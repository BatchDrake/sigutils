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

#ifndef _SIGUTILS_BLOCK_H
#define _SIGUTILS_BLOCK_H

#include <stdarg.h>
#include <stdint.h>
#include <util.h>
#include "types.h"

#define SU_BLOCK_STREAM_BUFFER_SIZE 4096

#define SU_BLOCK_PORT_READ_END_OF_STREAM          0
#define SU_BLOCK_PORT_READ_ERROR_NOT_INITIALIZED -1
#define SU_BLOCK_PORT_READ_ERROR_ACQUIRE         -2
#define SU_BLOCK_PORT_READ_ERROR_PORT_DESYNC     -3

typedef uint64_t su_off_t;

struct sigutils_stream {
  SUCOMPLEX *buffer;
  unsigned int size;  /* Stream size */
  unsigned int ptr;   /* Buffer pointer */
  unsigned int avail; /* Samples available for reading */

  su_off_t pos;       /* Stream position */
};

typedef struct sigutils_stream su_stream_t;

#define su_stream_INITIALIZER   \
{                               \
  NULL, /* buffer */            \
  0,    /* size */              \
  0,    /* ptr */               \
  0,    /* avail */             \
  0     /* post */              \
}

struct sigutils_block;

struct sigutils_block_port {
  su_off_t pos; /* Current reading position in this port */
  su_stream_t *stream; /* Stream position */
  struct sigutils_block *block; /* Input block */
};

typedef struct sigutils_block_port su_block_port_t;

#define su_block_port_INITIALIZER {0, NULL, NULL}

struct sigutils_block_class {
  const char *name;
  unsigned int in_size;
  unsigned int out_size;

  /* Generic constructor / destructor */
  SUBOOL (*ctor) (struct sigutils_block *block, void **private, va_list);
  void (*dtor) (void *private);

  /* This function gets called when more data is required */
  ssize_t (*acquire) (void *, su_stream_t *, su_block_port_t *);
};

typedef struct sigutils_block_class su_block_class_t;

enum sigutils_block_property_type {
  SU_BLOCK_PROPERTY_TYPE_ANY,
  SU_BLOCK_PROPERTY_TYPE_BOOL,
  SU_BLOCK_PROPERTY_TYPE_INTEGER,
  SU_BLOCK_PROPERTY_TYPE_FLOAT,
  SU_BLOCK_PROPERTY_TYPE_COMPLEX,
  SU_BLOCK_PROPERTY_TYPE_OBJECT
};

typedef enum sigutils_block_property_type su_block_property_type_t;

struct sigutils_block_property {
  su_block_property_type_t type;
  char *name;

  union {
    uint64_t *int_ptr;
    SUFLOAT *float_ptr;
    SUCOMPLEX *complex_ptr;
    SUBOOL *bool_ptr;
    void *generic_ptr;
  };
};

typedef struct sigutils_block_property su_block_property_t;

struct sigutils_block {
  su_block_class_t *class;
  void *private;

  su_block_port_t *in; /* Input ports */
  su_stream_t *out; /* Output streams */
  unsigned int decimation; /* Block decimation */

  /* Property list */
  PTR_LIST(su_block_property_t, property);
};

typedef struct sigutils_block su_block_t;

/* su_stream operations */
SUBOOL su_stream_init(su_stream_t *stream, size_t size);

void su_stream_finalize(su_stream_t *stream);

void su_stream_write(su_stream_t *stream, const SUCOMPLEX *data, size_t size);

size_t su_stream_get_contiguous(
    const su_stream_t *stream,
    SUCOMPLEX **start,
    size_t size);

size_t su_stream_advance_contiguous(su_stream_t *stream, size_t size);

su_off_t su_stream_tell(const su_stream_t *);

ssize_t su_stream_read(
    su_stream_t *stream,
    su_off_t off,
    SUCOMPLEX *data,
    size_t size);

/* su_block operations */
su_block_t *su_block_new(const char *, ...);

su_block_port_t *su_block_get_port(const su_block_t *, unsigned int);

su_stream_t *su_block_get_stream(const su_block_t *, unsigned int);

SUBOOL su_block_plug(
    su_block_t *source,
    unsigned int out_id,
    unsigned int in_id,
    su_block_t *sink);

su_block_property_t *su_block_lookup_property(
    const su_block_t *block,
    const char *name);

void *su_block_get_property_ref(
    const su_block_t *block,
    su_block_property_type_t type,
    const char *name);

SUBOOL su_block_set_property_ref(
    su_block_t *block,
    su_block_property_type_t type,
    const char *name,
    void *ptr);

void su_block_destroy(su_block_t *);

/* su_block_port operations */
SUBOOL su_block_port_plug(
    su_block_port_t *port,
    struct sigutils_block *block,
    unsigned int portid); /* Position initialized with current stream pos */

ssize_t su_block_port_read(su_block_port_t *port, SUCOMPLEX *obuf, size_t size);

/* Sometimes, a port connection may go out of sync. This fixes it */
SUBOOL su_block_port_resync(su_block_port_t *port);

SUBOOL su_block_port_is_plugged(const su_block_port_t *port);

void su_block_port_unplug(su_block_port_t *port);

/* su_block_class operations */
SUBOOL su_block_class_register(struct sigutils_block_class *class);

su_block_class_t *su_block_class_lookup(const char *name);

#endif /* _SIGUTILS_BLOCK_H */
