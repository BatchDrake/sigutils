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

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#  endif // __clang__
extern "C" {
#endif /* __cplusplus */

typedef struct sigutils_modem_class su_modem_class_t;

struct sigutils_modem_property {
  su_property_type_t type;
  char *name;

  union {
    uint64_t  as_int;
    SUFLOAT   as_float;
    SUCOMPLEX as_complex;
    SUBOOL    as_bool;
    void     *as_ptr;
    uint8_t   as_bytes[0];
  };
};

typedef struct sigutils_modem_property su_modem_property_t;

struct sigutils_modem_property_set {
  PTR_LIST(su_modem_property_t, property);
};

typedef struct sigutils_modem_property_set su_modem_property_set_t;

struct sigutils_modem;

struct sigutils_modem_class {
  const char *name;

  SUBOOL    (*ctor) (struct sigutils_modem *, void **);
  SUSYMBOL  (*read_sym) (struct sigutils_modem *, void *);
  SUCOMPLEX (*read_sample) (struct sigutils_modem *, void *);
  SUBOOL    (*onpropertychanged) (void *, const su_modem_property_t *);
  void      (*dtor) (void *);
};

struct sigutils_modem {
  struct sigutils_modem_class *classptr;
  void *privdata;

  SUFLOAT signal; /* signal indicator */
  SUFLOAT fec; /* FEC quality indicator */
  SUFLOAT snr; /* SNR ratio */
  su_block_t *source; /* Loaned */
  PTR_LIST(su_block_t, block); /* Owned */
  su_modem_property_set_t properties;
  su_property_set_t state_properties;
};

typedef struct sigutils_modem su_modem_t;

/**************** Modem class API **************************/
su_modem_class_t *su_modem_class_lookup(const char *name);

SUBOOL su_modem_class_register(su_modem_class_t *modem);

/**************** Modem property set API *******************/
void su_modem_property_set_init(su_modem_property_set_t *set);

su_modem_property_t *su_modem_property_set_lookup(
    const su_modem_property_set_t *set,
    const char *name);

su_modem_property_t *su_modem_property_set_assert_property(
    su_modem_property_set_t *set,
    const char *name,
    su_property_type_t type);

ssize_t su_modem_property_set_marshall(
    const su_modem_property_set_t *set,
    void *buffer,
    size_t buffer_size);

ssize_t su_modem_property_set_unmarshall(
    su_modem_property_set_t *dest,
    const void *buffer,
    size_t buffer_size);

SUBOOL su_modem_property_set_copy(
    su_modem_property_set_t *dest,
    const su_modem_property_set_t *src);

void su_modem_property_set_finalize(su_modem_property_set_t *set);

/****************** Modem API *******************/
su_modem_t *su_modem_new(const char *class_name);

SUBOOL su_modem_set_source(su_modem_t *modem, su_block_t *src);

SUBOOL su_modem_set_wav_source(su_modem_t *modem, const char *path);

SUBOOL su_modem_register_block(su_modem_t *modem, su_block_t *block);

SUBOOL su_modem_plug_to_source(su_modem_t *modem, su_block_t *first);

SUBOOL su_modem_expose_state_property(
    su_modem_t *modem,
    const char *name,
    su_property_type_t type,
    SUBOOL mandatory,
    void *ptr);

void *su_modem_get_state_property_ref(
    const su_modem_t *modem,
    const char *name,
    su_property_type_t type);

SUBOOL su_modem_load_state_property(
    su_modem_t *modem,
    const su_modem_property_t *prop);
SUBOOL su_modem_load_all_state_properties(su_modem_t *modem);
SUBOOL su_modem_set_int(su_modem_t *modem, const char *name, uint64_t val);
SUBOOL su_modem_set_float(su_modem_t *modem, const char *name, SUFLOAT val);
SUBOOL su_modem_set_complex(su_modem_t *modem, const char *name, SUCOMPLEX val);
SUBOOL su_modem_set_bool(su_modem_t *modem, const char *name, SUBOOL val);
SUBOOL su_modem_set_ptr(su_modem_t *modem, const char *name, void *);

const su_modem_property_t *su_modem_property_lookup(
    const su_modem_t *modem,
    const char *name);

const su_modem_property_t *
su_modem_property_lookup_typed(
    const su_modem_t *modem,
    const char *name,
    su_property_type_t type);

SUBOOL su_modem_set_properties(
    su_modem_t *modem,
    const su_modem_property_set_t *set);

SUBOOL su_modem_get_properties(
    const su_modem_t *modem,
    su_modem_property_set_t *set);

SUBOOL su_modem_start(su_modem_t *modem);

SUSYMBOL  su_modem_read(su_modem_t *modem);    /* Returns a stream of symbols */
SUCOMPLEX su_modem_read_sample(su_modem_t *modem);
SUFLOAT   su_modem_get_fec(su_modem_t *modem); /* Returns FEC quality */
SUFLOAT   su_modem_get_snr(su_modem_t *modem); /* Returns SNR magnitude */
SUFLOAT   su_modem_get_signal(su_modem_t *modem); /* Signal indicator */

/* This functions are to be used by modem implementations */
void su_modem_set_fec(su_modem_t *modem, SUFLOAT fec);
void su_modem_set_snr(su_modem_t *modem, SUFLOAT snr);
void su_modem_set_signal(su_modem_t *modem, SUFLOAT signal);

void su_modem_destroy(su_modem_t *modem);

#ifdef __cplusplus
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif // __clang__
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_MODEM_H */
