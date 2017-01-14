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

#ifndef _SIGUTILS_PROPERTY_H
#define _SIGUTILS_PROPERTY_H

#include <stdint.h>

#include "types.h"

enum sigutils_property_type {
  SU_PROPERTY_TYPE_ANY,
  SU_PROPERTY_TYPE_BOOL,
  SU_PROPERTY_TYPE_INTEGER,
  SU_PROPERTY_TYPE_FLOAT,
  SU_PROPERTY_TYPE_COMPLEX,
  SU_PROPERTY_TYPE_OBJECT
};

typedef enum sigutils_property_type su_property_type_t;

struct sigutils_property {
  su_property_type_t type;
  char *name;

  union {
    uint64_t *int_ptr;
    SUFLOAT *float_ptr;
    SUCOMPLEX *complex_ptr;
    SUBOOL *bool_ptr;
    void *generic_ptr;
  };
};

typedef struct sigutils_property su_property_t;

struct sigutils_property_set {
  PTR_LIST(su_property_t, property);
};

typedef struct sigutils_property_set su_property_set_t;

const char *su_property_type_to_string(su_property_type_t type);

/* Property API */
su_property_t *su_property_new(const char *name, su_property_type_t type, void *p);
void su_property_destroy(su_property_t *prop);

/* Property set API */
void su_property_set_init(su_property_set_t *set);
su_property_t *su_property_set_lookup(const su_property_set_t *set, const char *name);
su_property_t *su_property_set_assert_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type);
void su_property_set_finalize(su_property_set_t *set);

#endif /* _SIGUTILS_PROPERTY_H */
