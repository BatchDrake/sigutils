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
#include <sigutils/util/util.h>

#include <sigutils/defs.h>
#include <sigutils/property.h>
#include <sigutils/types.h>

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
SU_GETTER(
    su_stream,
    SUSCOUNT,
    get_contiguous,
    SUCOMPLEX **start,
    SUSCOUNT size);
SU_GETTER(su_stream, su_off_t, tell);
SU_GETTER(
    su_stream,
    SUSDIFF,
    read,
    su_off_t off,
    SUCOMPLEX *data,
    SUSCOUNT size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_BLOCK_H */
