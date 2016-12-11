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

#include <string.h>
#include <stdlib.h>
#include "modem.h"

void
su_modem_property_set_init(su_modem_property_set_t *set)
{
  memset(set, 0, sizeof (su_modem_property_set_t));
}

void
su_modem_property_destroy(su_modem_property_t *prop)
{
  if (prop->name != NULL)
    free(prop->name);

  free(prop);
}

su_modem_property_t *
su_modem_property_new(const char *name, su_modem_property_type_t type)
{
  su_modem_property_t *new = NULL;

  if ((new = calloc(1, sizeof (su_modem_property_t))) == NULL)
    goto fail;

  if ((new->name = strdup(name)) == NULL)
    goto fail;

  new->type = type;

  /* Contents already initialized to zero */
  return new;

fail:
  if (new != NULL)
    su_modem_property_destroy(new);

  return NULL;
}

su_modem_property_t *
su_modem_property_lookup(const su_modem_property_set_t *set, const char *name)
{
  unsigned int i;
  su_modem_property_t *this = NULL;

  FOR_EACH_PTR(this, i, set->property)
    if (strcmp(this->name, name) == 0)
      return this;

  return NULL;
}

SUPRIVATE const char *
su_modem_property_type_to_string(su_modem_property_type_t type)
{
  switch(type) {
    case SU_MODEM_PROPERTY_TYPE_ANY:
      return "(any)";

    case SU_MODEM_PROPERTY_TYPE_BOOL:
      return "bool";

    case SU_MODEM_PROPERTY_TYPE_INTEGER:
      return "int";

    case SU_MODEM_PROPERTY_TYPE_FLOAT:
      return "float";

    case SU_MODEM_PROPERTY_TYPE_COMPLEX:
      return "complex";

    case SU_MODEM_PROPERTY_TYPE_OBJECT:
      return "object";

    default:
      return "unknown";
  }
}

su_modem_property_t *
su_modem_property_assert(
    su_modem_property_set_t *set,
    const char *name,
    su_modem_property_type_t type)
{
  su_modem_property_t *prop = NULL;

  if ((prop = su_modem_property_lookup(set, name)) == NULL) {
    if ((prop = su_modem_property_new(name, type)) == NULL) {
      SU_ERROR(
          "failed to create new %s property",
          su_modem_property_type_to_string(type));
      return NULL;
    }

    if (PTR_LIST_APPEND_CHECK(set->property, prop) == -1) {
      SU_ERROR(
          "failed to append new %s property",
          su_modem_property_type_to_string(type));
      su_modem_property_destroy(prop);
      return NULL;
    }
  } else if (prop->type != type) {
    SU_ERROR(
        "property `%s' found, mismatching type (req: %s, found: %s)\n",
        name,
        su_modem_property_type_to_string(type),
        su_modem_property_type_to_string(prop->type));
    return NULL;
  }

  return prop;
}

SUPRIVATE ssize_t
su_modem_property_get_value_marshalled_size(su_modem_property_type_t type)
{
  switch (type) {
    case SU_MODEM_PROPERTY_TYPE_ANY:
      return 0;

    case SU_MODEM_PROPERTY_TYPE_BOOL:
      return 1;

    case SU_MODEM_PROPERTY_TYPE_INTEGER:
      return sizeof(uint64_t);

    case SU_MODEM_PROPERTY_TYPE_FLOAT:
      return sizeof(SUFLOAT);

    case SU_MODEM_PROPERTY_TYPE_COMPLEX:
      return sizeof(SUCOMPLEX);

    case SU_MODEM_PROPERTY_TYPE_OBJECT:
      SU_ERROR("object properties cannot be marshalled\n");
      return -1;

    default:
      return -1;
  }
}

SUPRIVATE ssize_t
su_modem_property_get_marshalled_size(const su_modem_property_t *prop)
{
  size_t size = 0;
  ssize_t field_size = 0;
  size_t tmp_size = 0;
  size += sizeof(uint8_t); /* Property type */
  size += sizeof(uint8_t); /* Property name size  */

  if ((tmp_size = strlen(prop->name) + 1) > 255) {
    SU_ERROR("property name is too long: %d, can't serialize\n", tmp_size);
    return -1;
  }

  size += tmp_size; /* Property name */

  if ((field_size = su_modem_property_get_value_marshalled_size(prop->type))
      == -1) {
    SU_ERROR(
        "cannot marshall properties of type `%s'\n",
        su_modem_property_type_to_string(prop->type));
    return -1;
  }

  size += field_size;

  return size;
}


SUPRIVATE ssize_t
su_modem_property_marshall(
    const su_modem_property_t *prop,
    void *buffer,
    size_t buffer_size)
{
  uint8_t *as_bytes = NULL;
  off_t ptr = 0;
  size_t tmp_size = 0;
  ssize_t prop_size = 0;

  if ((prop_size = su_modem_property_get_marshalled_size(prop)) == -1) {
    SU_ERROR("cannot marshall property `%s'\n", prop->name);
    return -1;
  }

  if (buffer == NULL || buffer_size == 0)
    return prop_size;
  else if (buffer_size < prop_size)
    return -1;

  as_bytes = (uint8_t *) buffer;

  tmp_size = strlen(prop->name) + 1;

  as_bytes[ptr++] = prop->type;
  as_bytes[ptr++] = tmp_size;

  /* Marshall property name */
  memcpy(&as_bytes[ptr], prop->name, tmp_size);
  ptr += tmp_size;

  /* Marshall property contents */
  tmp_size = su_modem_property_get_value_marshalled_size(prop->type);
  memcpy(&as_bytes[ptr], prop->as_bytes, tmp_size);
  ptr += tmp_size;

  return ptr;
}

SUPRIVATE ssize_t
su_modem_property_unmarshall(
    su_modem_property_t *prop,
    const void *buffer,
    size_t buffer_size)
{
  su_modem_property_type_t type;
  const uint8_t *as_bytes = NULL;
  size_t name_size = 0;
  size_t value_size = 0;
  off_t ptr = 0;
  const char *name_ptr = NULL;
  const uint8_t *value = NULL;

  /* Minimum size: type + name size + null terminator */
  if (buffer_size < 3)
    goto corrupted;

  as_bytes = (const uint8_t *) buffer;
  type = as_bytes[ptr++];
  name_size = as_bytes[ptr++];

  /* Does the name fit in the buffer? */
  if (ptr + name_size > buffer_size)
    goto corrupted;

  /* Is it a null-terminated string? */
  name_ptr = (const char *) &as_bytes[ptr];
  if (name_ptr[name_size - 1] != 0)
    goto corrupted;
  ptr += name_size;

  /* Does field contents fit in the buffer? */
  value_size = su_modem_property_get_value_marshalled_size(type);
  if (ptr + value_size > buffer_size)
    goto corrupted;
  value = (const char *) &as_bytes[ptr];
  ptr += value_size;

  /* All required data is available, initialize property */
  if ((prop->name = strdup(name_ptr)) == NULL) {
    SU_ERROR("cannot allocate memory for property name\n");
    return -1;
  }

  prop->type = type;
  memcpy(prop->as_bytes, value, value_size);

  return ptr;

corrupted:
  SU_ERROR("corrupted property\n");

  return -1;
}

SUPRIVATE size_t
su_modem_property_set_get_marshalled_size(const su_modem_property_set_t *set)
{
  size_t size = 0;
  ssize_t prop_size = 0;
  unsigned int i = 0;
  const su_modem_property_t *this = NULL;

  size = sizeof(uint16_t); /* Property counter */

  FOR_EACH_PTR(this, i, set->property)
    if ((prop_size = su_modem_property_get_marshalled_size(this)) > 0)
      size += prop_size;

  return size;
}

ssize_t
su_modem_property_set_marshall(
    const su_modem_property_set_t *set,
    void *buffer,
    size_t buffer_size)
{
  size_t marshalled_size = 0;
  ssize_t prop_size;
  unsigned int i = 0;
  const su_modem_property_t *this = NULL;
  off_t ptr = 0;
  uint8_t *as_bytes = NULL;
  unsigned int count = 0;

  marshalled_size = su_modem_property_set_get_marshalled_size(set);

  if (buffer == NULL || buffer_size == 0)
    return marshalled_size;

  if (buffer_size < marshalled_size)
    return -1;

  as_bytes = (uint8_t *) buffer;

  ptr = 2;

  FOR_EACH_PTR(this, i, set->property) {
    if ((prop_size = su_modem_property_get_marshalled_size(this)) > 0) {
      if ((prop_size = su_modem_property_marshall(
          this,
          &as_bytes[ptr],
          buffer_size - ptr)) < 0) {
        SU_ERROR("failed to marshall property `%s'\n", this->name);
        return -1;
      }

      ptr += prop_size;
      if ((uint16_t) ++count == 0) {
        SU_ERROR("too many properties (%d)\n", count);
        return -1;
      }
    } else {
      SU_WARNING("cannot marshall property `%s', skipping\n", this->name);
    }
  }

  *((uint16_t *) as_bytes) = count;

  return ptr;
}

ssize_t
su_modem_property_set_unmarshall(
    su_modem_property_set_t *set,
    const void *buffer,
    size_t buffer_size)
{
  uint16_t count = 0;
  unsigned int i = 0;
  off_t ptr = 0;
  ssize_t prop_size = 0;
  su_modem_property_t *prop = NULL;
  const uint8_t *as_bytes = NULL;

  if (buffer_size < 2)
    goto corrupted;

  as_bytes = (const uint8_t *) buffer;
  count = *((const uint16_t *) as_bytes);

  ptr += 2;

  su_modem_property_set_init(set);

  for (i = 0; i < count; ++i) {
    if ((prop = calloc(1, sizeof (su_modem_property_t))) == NULL) {
      SU_ERROR("cannot allocate new property\n");
      return -1;
    }

    if ((prop_size = su_modem_property_unmarshall(
        prop,
        &as_bytes[ptr],
        buffer_size - ptr)) < 0) {
      /* Property can be easily freed here */
      free(prop);
      goto corrupted;
    }

    ptr += prop_size;

    /* TODO: what happens if there are two properties with the same name? */
    if (PTR_LIST_APPEND_CHECK(set->property, prop) == -1) {
      SU_ERROR("cannot append new property\n");
      su_modem_property_destroy(prop);
      return -1;
    }
  }

  return ptr;

corrupted:
  SU_ERROR("corrupted marshalled properties\n");

  return -1;
}
