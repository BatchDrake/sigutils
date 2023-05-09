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

#ifndef _SIGUTILS_PROPERTY_H
#define _SIGUTILS_PROPERTY_H

#include <stdint.h>

#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
  SUBOOL mandatory;

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
su_property_t *su_property_new(
    const char *name,
    su_property_type_t type,
    SUBOOL mandatory,
    void *p);

/* Property set API */
void su_property_set_init(su_property_set_t *set);
su_property_t *su_property_set_lookup(
    const su_property_set_t *set,
    const char *name);
su_property_t *su_property_set_assert_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type);
su_property_t *su_property_set_assert_mandatory_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type);
su_property_t *__su_property_set_assert_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type,
    SUBOOL mandatory);

void su_property_set_finalize(su_property_set_t *set);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_PROPERTY_H */
