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

#ifndef _SIGUTILS_MODEM_H
#define _SIGUTILS_MODEM_H

#include <util.h>
#include "types.h"
#include "block.h"

struct sigutils_modem;
struct sigutils_modem_property;

struct sigutils_modem_class {
  const char *name;

  SUBOOL   (*ctor) (struct sigutils_modem *, void **);
  SUSYMBOL (*read) (void *);
  SUBOOL   (*onpropertychanged) (void *, const struct sigutils_modem_property);
  void     (*dtor) (void *);
};

enum sigutils_modem_property_type {
  SU_MODEM_PROPERTY_TYPE_ANY,
  SU_MODEM_PROPERTY_TYPE_BOOL,
  SU_MODEM_PROPERTY_TYPE_INTEGER,
  SU_MODEM_PROPERTY_TYPE_FLOAT,
  SU_MODEM_PROPERTY_TYPE_COMPLEX,
  SU_MODEM_PROPERTY_TYPE_OBJECT
};

typedef enum sigutils_modem_property_type su_modem_property_type_t;

struct sigutils_modem_property {
  su_modem_property_type_t type;
  const char *name;

  union {
    uint64_t  as_int;
    SUFLOAT   as_float;
    SUCOMPLEX as_complex;
    SUBOOL    as_bool;
    void     *as_ptr;
  };
};

typedef struct sigutils_modem_property su_modem_property_t;

struct sigutils_modem_property_set {
  PTR_LIST(su_modem_property_t, property);
};

typedef struct sigutils_modem_property_set su_modem_property_set_t;

struct sigutils_modem {
  struct sigutils_modem_class *class;
  void *private;

  SUFLOAT signal; /* signal indicator */
  SUFLOAT fec; /* FEC quality indicator */
  SUFLOAT snr; /* SNR ratio */
  su_block_t *source; /* Loaned */
  PTR_LIST(su_block_t, block); /* Owned */
  su_modem_property_set_t properties;
};

typedef struct sigutils_modem_t su_modem_t;

/**************** Modem property API *******************/
SUBOOL su_modem_property_set_init(su_modem_property_set_t *set);

su_modem_property_t *su_modem_property_lookup(
    su_modem_property_set_t *set,
    const char *name);

su_modem_property_t *su_modem_property_assert(
    su_modem_property_set_t *set,
    const char *name,
    su_modem_property_type_t type);

ssize_t su_modem_property_set_marshall(
    const su_modem_property_set_t *set,
    void *buffer,
    size_t buffer_size);

SUBOOL su_modem_property_set_unmarshall(
    su_modem_property_set_t *dest,
    const void *buffer,
    size_t buffer_size);

SUBOOL su_modem_property_set_load(
    su_modem_property_set_t *set,
    const char *path);

SUBOOL su_modem_property_set_save(
    const su_modem_property_set_t *set,
    const char *path);

SUBOOL su_modem_property_set_copy(
    su_modem_property_set_t *dest,
    const su_modem_property_set_t *src);

void su_modem_property_set_finalize(su_modem_property_set_t *set);

/****************** Modem API *******************/
su_modem_t *su_modem_new(const char *class_name);

SUBOOL su_modem_set_source(su_modem_t *modem, su_block_t *src);

SUBOOL su_modem_set_wav_source(su_modem_t *modem, const char *path);

SUBOOL su_modem_set_int(su_modem_t *modem, const char *name, uint64_t val);
SUBOOL su_modem_set_float(su_modem_t *modem, const char *name, SUFLOAT val);
SUBOOL su_modem_set_complex(su_modem_t *modem, const char *name, SUCOMPLEX val);
SUBOOL su_modem_set_bool(su_modem_t *modem, const char *name, SUBOOL val);
SUBOOL su_modem_set_ptr(su_modem_t *modem, const char *name, void *);

SUBOOL su_modem_set_properties(
    su_modem_t *modem,
    const su_modem_property_set_t *set);

SUBOOL su_modem_get_properties(
    su_modem_t *modem,
    su_modem_property_set_t *set);

SUBOOL su_modem_run(su_modem_t *modem);

SUSYMBOL su_modem_read(su_modem_t *modem);
SUFLOAT  su_modem_get_fec(su_modem_t *modem);
SUFLOAT  su_modem_get_snr(su_modem_t *modem);
SUFLOAT  su_modem_get_signal(su_modem_t *modem);

void su_modem_destroy(su_modem_t *modem);

#endif /* _SIGUTILS_MODEM_H */
