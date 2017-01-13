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

PTR_LIST(SUPRIVATE su_modem_class_t, modem_class);

/**************** Modem class API **********************/
su_modem_class_t *
su_modem_class_lookup(const char *name)
{
  unsigned int i = 0;
  su_modem_class_t *this = NULL;

  FOR_EACH_PTR(this, i, modem_class)
    if (strcmp(this->name, name) == 0)
      return this;

  return NULL;
}

SUBOOL
su_modem_class_register(su_modem_class_t *modem)
{
  if (su_modem_class_lookup(modem->name) != NULL) {
    SU_ERROR("cannot register modem class: %s already exists\n", modem->name);
    return SU_FALSE;
  }

  if (PTR_LIST_APPEND_CHECK(modem_class, modem) == -1) {
    SU_ERROR("cannot apend modem to modem list\n");
    return SU_FALSE;
  }

  return SU_TRUE;
}

/**************** Modem property API *******************/
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
su_modem_property_set_lookup(const su_modem_property_set_t *set, const char *name)
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

SUBOOL
su_modem_property_copy(
    su_modem_property_t *dst,
    const su_modem_property_t *src)
{
  ssize_t size;

  if (dst->type != src->type) {
    SU_ERROR("cannot overwrite property of mismatching type\n");
    return SU_FALSE;
  }

  if ((size = su_modem_property_get_value_marshalled_size(src->type)) == -1) {
    SU_ERROR(
        "objects of type %s cannot be copied\n",
        su_modem_property_type_to_string(src->type));
    return SU_FALSE;
  }

  memcpy(&dst->as_bytes, src->as_bytes, size);

  return SU_TRUE;
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

/****************** Property Set API *******************/
void
su_modem_property_set_init(su_modem_property_set_t *set)
{
  memset(set, 0, sizeof (su_modem_property_set_t));
}

su_modem_property_t *
su_modem_property_set_assert_property(
    su_modem_property_set_t *set,
    const char *name,
    su_modem_property_type_t type)
{
  su_modem_property_t *prop = NULL;

  if ((prop = su_modem_property_set_lookup(set, name)) == NULL) {
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
  SU_ERROR("corrupted marshalled properties\n(su_modem_t *modem,");

  return -1;
}

SUBOOL
su_modem_property_set_copy(
    su_modem_property_set_t *dest,
    const su_modem_property_set_t *src)
{
  unsigned int i = 0;
  su_modem_property_t *this = NULL;
  su_modem_property_t *dst_prop = NULL;

  FOR_EACH_PTR(this, i, src->property) {
    if ((dst_prop = su_modem_property_set_assert_property(dest, this->name, this->type))
        == NULL) {
      SU_ERROR("failed to assert property `%s'\n", this->name);
      return SU_FALSE;
    }

    if (!su_modem_property_copy(dst_prop, this)) {
      SU_ERROR("failed to copy property `%s'\n", this->name);
      return SU_FALSE;
    }
  }

  return SU_TRUE;
}

void
su_modem_property_set_finalize(su_modem_property_set_t *set)
{
  unsigned int i = 0;
  su_modem_property_t *this = NULL;

  FOR_EACH_PTR(this, i, set->property)
    su_modem_property_destroy(this);

  if (set->property_list != NULL)
    free(set->property_list);
}

/****************** Modem API *******************/
void
su_modem_destroy(su_modem_t *modem)
{
  unsigned int i = 0;
  su_block_t *this = NULL;

  if (modem->private != NULL)
    (modem->class->dtor) (modem->private);

  FOR_EACH_PTR(this, i, modem->block)
    su_block_destroy(this);

  if (modem->block_list != NULL)
    free(modem->block_list);

  su_modem_property_set_finalize(&modem->properties);

  free(modem);
}

su_modem_t *
su_modem_new(const char *class_name)
{
  su_modem_t *new = NULL;
  su_modem_class_t *class = NULL;

  if ((class = su_modem_class_lookup(class_name)) == NULL) {
    SU_ERROR("modem class `%s' not registered\n", class_name);
    goto fail;
  }

  if ((new = calloc(1, sizeof(su_modem_t))) == NULL)
    goto fail;

  new->class = class;

  return new;

fail:
  if (new != NULL)
    su_modem_destroy(new);

  return NULL;
}

SUBOOL
su_modem_set_source(su_modem_t *modem, su_block_t *src)
{
  if (modem->private != NULL) {
    SU_ERROR("cannot set source while modem has started\n");
    return SU_FALSE;
  }

  modem->source = src;

  return SU_TRUE;
}

SUBOOL
su_modem_set_wav_source(su_modem_t *modem, const char *path)
{
  su_block_t *wav_block = NULL;
  const uint64_t *samp_rate = NULL;

  if ((wav_block = su_block_new("wavfile", path)) == NULL)
    goto fail;

  if ((samp_rate = su_block_get_property_ref(
      wav_block,
      SU_PROPERTY_TYPE_INTEGER,
      "samp_rate")) == NULL) {
    SU_ERROR("failed to acquire wav file sample rate\n");
    goto fail;
  }

  if (!su_modem_register_block(modem, wav_block)) {
    SU_ERROR("failed to register wav source\n");
    su_block_destroy(wav_block);
    goto fail;
  }
  if (!su_modem_set_int(modem, "samp_rate", *samp_rate)) {
    SU_ERROR("failed to set modem sample rate\n");
    goto fail;
  }

  if (!su_modem_set_source(modem, wav_block))
    goto fail;

  return SU_TRUE;

fail:
  if (wav_block != NULL)
    su_block_destroy(wav_block);

  return SU_FALSE;
}

SUBOOL
su_modem_set_int(su_modem_t *modem, const char *name, uint64_t val)
{
  su_modem_property_t *prop = NULL;
  uint64_t old;

  if ((prop = su_modem_property_set_assert_property(
      &modem->properties,
      name,
      SU_MODEM_PROPERTY_TYPE_INTEGER)) == NULL)
    return SU_FALSE;

  old = prop->as_int;
  prop->as_int = val;

  if (!(modem->class->onpropertychanged)(modem->private, prop)) {
    SU_ERROR("change of property `%s' rejected\n", name);
    prop->as_int = old;

    return SU_FALSE;
  }

  return SU_TRUE;
}

SUBOOL
su_modem_register_block(su_modem_t *modem, su_block_t *block)
{
  return PTR_LIST_APPEND_CHECK(modem->block, block) != -1;
}

SUBOOL
su_modem_set_float(su_modem_t *modem, const char *name, SUFLOAT val)
{
  su_modem_property_t *prop = NULL;
  SUFLOAT old;

  if ((prop = su_modem_property_set_assert_property(
      &modem->properties,
      name,
      SU_MODEM_PROPERTY_TYPE_FLOAT)) == NULL)
    return SU_FALSE;

  old = prop->as_float;
  prop->as_float = val;

  if (!(modem->class->onpropertychanged)(modem->private, prop)) {
    SU_ERROR("change of property `%s' rejected\n", name);
    prop->as_float = old;

    return SU_FALSE;
  }


  return SU_TRUE;
}

SUBOOL
su_modem_set_complex(su_modem_t *modem, const char *name, SUCOMPLEX val)
{
  su_modem_property_t *prop = NULL;
  SUCOMPLEX old;

  if ((prop = su_modem_property_set_assert_property(
      &modem->properties,
      name,
      SU_MODEM_PROPERTY_TYPE_COMPLEX)) == NULL)
    return SU_FALSE;

  old = prop->as_complex;
  prop->as_complex = val;

  if (!(modem->class->onpropertychanged)(modem->private, prop)) {
    SU_ERROR("change of property `%s' rejected\n", name);
    prop->as_complex = old;

    return SU_FALSE;
  }


  return SU_TRUE;
}

SUBOOL
su_modem_set_bool(su_modem_t *modem, const char *name, SUBOOL val)
{
  su_modem_property_t *prop = NULL;
  SUBOOL old;

  if ((prop = su_modem_property_set_assert_property(
      &modem->properties,
      name,
      SU_MODEM_PROPERTY_TYPE_BOOL)) == NULL)
    return SU_FALSE;

  old = prop->as_bool;
  prop->as_bool = val;

  if (!(modem->class->onpropertychanged)(modem->private, prop)) {
    SU_ERROR("change of property `%s' rejected\n", name);
    prop->as_bool = old;

    return SU_FALSE;
  }


  return SU_TRUE;
}

SUBOOL
su_modem_set_ptr(su_modem_t *modem, const char *name, void *val)
{
  su_modem_property_t *prop = NULL;
  void *old;

  if ((prop = su_modem_property_set_assert_property(
      &modem->properties,
      name,
      SU_MODEM_PROPERTY_TYPE_OBJECT)) == NULL)
    return SU_FALSE;

  old = prop->as_ptr;
  prop->as_ptr = val;

  if (!(modem->class->onpropertychanged)(modem->private, prop)) {
    SU_ERROR("change of property `%s' rejected\n", name);
    prop->as_ptr = old;

    return SU_FALSE;
  }


  return SU_TRUE;
}

const su_modem_property_t *
su_modem_property_lookup(const su_modem_t *modem, const char *name)
{
  return su_modem_property_set_lookup(&modem->properties, name);
}

const su_modem_property_t *
su_modem_property_lookup_typed(
    const su_modem_t *modem,
    const char *name,
    su_modem_property_type_t type)
{
  const su_modem_property_t *prop = NULL;

  if ((prop = su_modem_property_lookup(modem, name)) == NULL)
    return NULL;
  else if (prop->type != type) {
    SU_ERROR(
        "Property `%s' is of type `%s', but `%s' was expected\n",
        name,
        su_modem_property_type_to_string(prop->type),
        su_modem_property_type_to_string(type));
    return NULL;
  }

  return prop;
}

SUBOOL
su_modem_set_properties(su_modem_t *modem, const su_modem_property_set_t *set)
{
  unsigned int i = 0;
  su_modem_property_t *this = NULL;
  su_modem_property_t *dst_prop = NULL;

  FOR_EACH_PTR(this, i, set->property) {
    if ((dst_prop = su_modem_property_set_assert_property(
        &modem->properties,
        this->name,
        this->type)) == NULL) {
      SU_ERROR("failed to assert property `%s'\n", this->name);
      return SU_FALSE;
    }

    /* This is not necessarily an error */
    if (!(modem->class->onpropertychanged)(modem->private, this)) {
      SU_WARNING("property `%s' cannot be changed\n", this->name);
      continue;
    }

    if (!su_modem_property_copy(dst_prop, this)) {
      SU_ERROR("failed to copy property `%s'\n", this->name);
      return SU_FALSE;
    }
  }

  return SU_TRUE;
}

SUBOOL
su_modem_plug_to_source(su_modem_t *modem, su_block_t *first)
{
  if (modem->source == NULL) {
    SU_ERROR("source not defined\n");
    return SU_FALSE;
  }

  return su_block_plug(modem->source, 0, 0, first);
}

SUBOOL
su_modem_get_properties(const su_modem_t *modem, su_modem_property_set_t *set)
{
  return su_modem_property_set_copy(set, &modem->properties);
}

SUBOOL
su_modem_start(su_modem_t *modem)
{
  if (modem->source == NULL) {
    SU_ERROR("cannot start modem: source not defined\n");
    return SU_FALSE;
  }

  if (!(modem->class->ctor)(modem, &modem->private)) {
    SU_ERROR("failed to start modem\n");
    modem->private = NULL;

    return SU_FALSE;
  }

  return SU_TRUE;
}

SUSYMBOL
su_modem_read(su_modem_t *modem)
{
  if (modem->private == NULL) {
    SU_ERROR("modem not started\n");
    return SU_EOS;
  }

  return (modem->class->read_sym)(modem, modem->private);
}

SUCOMPLEX
su_modem_read_sample(su_modem_t *modem)
{
  if (modem->private == NULL) {
    SU_ERROR("modem not started\n");
    return SU_EOS;
  }

  return (modem->class->read_sample)(modem, modem->private);
}

SUFLOAT
su_modem_get_fec(su_modem_t *modem)
{
  if (modem->private == NULL) {
    SU_ERROR("modem not started\n");
    return 0;
  }

  return modem->fec;
}

SUFLOAT
su_modem_get_snr(su_modem_t *modem)
{
  if (modem->private == NULL) {
    SU_ERROR("modem not started\n");
    return 0;
  }

  return modem->snr;
}

SUFLOAT
su_modem_get_signal(su_modem_t *modem)
{
  if (modem->private == NULL) {
    SU_ERROR("modem not started\n");
    return 0;
  }

  return modem->signal;
}

void
su_modem_set_fec(su_modem_t *modem, SUFLOAT fec)
{
  modem->fec = fec;
}

void
su_modem_set_snr(su_modem_t *modem, SUFLOAT snr)
{
  modem->snr = snr;
}

void
su_modem_set_signal(su_modem_t *modem, SUFLOAT signal)
{
  modem->signal = signal;
}

