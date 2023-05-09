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

#include <stdlib.h>
#include <string.h>
#include <sigutils/util/util.h>
#define SU_LOG_LEVEL "property"

#include <sigutils/log.h>
#include <sigutils/property.h>

/************************** su_block_property API ****************************/
su_property_t *
su_property_new(const char *name, su_property_type_t type, SUBOOL m, void *p)
{
  su_property_t *new = NULL;
  char *namedup = NULL;

  if ((new = malloc(sizeof(su_property_t))) == NULL)
    goto fail;

  if ((namedup = strdup(name)) == NULL)
    goto fail;

  new->mandatory = m;
  new->name = namedup;
  new->type = type;
  new->generic_ptr = p;

  return new;

fail:
  if (new != NULL)
    free(new);

  if (namedup != NULL)
    free(namedup);

  return NULL;
}

void
su_property_destroy(su_property_t *prop)
{
  if (prop->name != NULL)
    free(prop->name);

  free(prop);
}

/************************** su_block_property_set API *************************/
void
su_property_set_init(su_property_set_t *set)
{
  memset(set, 0, sizeof(su_property_set_t));
}

su_property_t *
su_property_set_lookup(const su_property_set_t *set, const char *name)
{
  su_property_t *this = NULL;

  FOR_EACH_PTR(this, set, property)
  if (strcmp(this->name, name) == 0)
    return this;

  return NULL;
}

const char *
su_property_type_to_string(su_property_type_t type)
{
  switch (type) {
    case SU_PROPERTY_TYPE_ANY:
      return "(any)";

    case SU_PROPERTY_TYPE_BOOL:
      return "bool";

    case SU_PROPERTY_TYPE_INTEGER:
      return "int";

    case SU_PROPERTY_TYPE_FLOAT:
      return "float";

    case SU_PROPERTY_TYPE_COMPLEX:
      return "complex";

    case SU_PROPERTY_TYPE_OBJECT:
      return "object";

    default:
      return "unknown";
  }
}

su_property_t *
__su_property_set_assert_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type,
    SUBOOL mandatory)
{
  su_property_t *prop = NULL;

  if ((prop = su_property_set_lookup(set, name)) == NULL) {
    if ((prop = su_property_new(name, type, mandatory, NULL)) == NULL) {
      SU_ERROR(
          "failed to create new %s property",
          su_property_type_to_string(type));
      return NULL;
    }

    if (PTR_LIST_APPEND_CHECK(set->property, prop) == -1) {
      SU_ERROR(
          "failed to append new %s property",
          su_property_type_to_string(type));
      su_property_destroy(prop);
      return NULL;
    }
  } else if (prop->type != type) {
    SU_ERROR(
        "property `%s' found, mismatching type (req: %s, found: %s)\n",
        name,
        su_property_type_to_string(type),
        su_property_type_to_string(prop->type));
    return NULL;
  }

  return prop;
}

su_property_t *
su_property_set_assert_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type)
{
  return __su_property_set_assert_property(set, name, type, SU_FALSE);
}

su_property_t *
su_property_set_assert_mandatory_property(
    su_property_set_t *set,
    const char *name,
    su_property_type_t type)
{
  return __su_property_set_assert_property(set, name, type, SU_TRUE);
}

void
su_property_set_finalize(su_property_set_t *set)
{
  su_property_t *this = NULL;

  FOR_EACH_PTR(this, set, property)
  su_property_destroy(this);

  if (set->property_list != NULL)
    free(set->property_list);
}
