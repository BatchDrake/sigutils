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

#define SU_LOG_DOMAIN "encoder"

#include <string.h>
#include "log.h"
#include "encoder.h"

PTR_LIST_CONST(SUPRIVATE struct sigutils_encoder_class, class);

const struct sigutils_encoder_class *
su_encoder_class_lookup(const char *name)
{
  unsigned int i;

  for (i = 0; i < class_count; ++i)
    if (strcmp(class_list[i]->name, name) == 0)
      return class_list[i];

  return NULL;
}

SUBOOL
su_encoder_class_register(const struct sigutils_encoder_class *class)
{
  SU_TRYCATCH(class->name   != NULL, return SU_FALSE);
  SU_TRYCATCH(class->ctor   != NULL, return SU_FALSE);
  SU_TRYCATCH(class->encode != NULL, return SU_FALSE);
  SU_TRYCATCH(class->decode != NULL, return SU_FALSE);
  SU_TRYCATCH(class->dtor   != NULL, return SU_FALSE);

  SU_TRYCATCH(su_encoder_class_lookup(class->name) == NULL, return SU_FALSE);
  SU_TRYCATCH(
      PTR_LIST_APPEND_CHECK(class, (void *) class) != -1,
      return SU_FALSE);

  return SU_TRUE;
}

void
su_encoder_destroy(su_encoder_t *encoder)
{
  if (encoder->class != NULL)
    (encoder->class->dtor)(encoder->private);

  free(encoder);
}

void
su_encoder_set_direction(su_encoder_t *encoder, enum su_encoder_direction dir)
{
  encoder->direction = dir;
}

SUSYMBOL
su_encoder_feed(su_encoder_t *encoder, SUSYMBOL x)
{
  switch (encoder->direction) {
    case SU_ENCODER_DIRECTION_FORWARDS:
      return (encoder->class->encode) (encoder, encoder->private, x);
    case SU_ENCODER_DIRECTION_BACKWARDS:
      return (encoder->class->decode) (encoder, encoder->private, x);
  }

  return SU_NOSYMBOL;
}

su_encoder_t *
su_encoder_new(const char *classname, unsigned int bits, ...)
{
  su_encoder_t *new = NULL;
  va_list ap;

  va_start(ap, bits);

  SU_TRYCATCH(new = calloc(1, sizeof(su_encoder_t)), goto fail);

  new->direction = SU_ENCODER_DIRECTION_FORWARDS;
  new->bits = bits;

  if ((new->class = su_encoder_class_lookup(classname)) == NULL) {
    SU_ERROR("No such encoder class `%s'\n", classname);
    goto fail;
  }

  if (!(new->class->ctor) (new, &new->private, ap)) {
    SU_ERROR("Failed to construct `%s'\n", classname);
    goto fail;
  }

  va_end(ap);

fail:
  if (new != NULL)
    su_encoder_destroy(new);

  return NULL;
}
