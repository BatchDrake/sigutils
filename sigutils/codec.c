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

#define SU_LOG_DOMAIN "codec"

#include <string.h>
#include "log.h"
#include "codec.h"

PTR_LIST_CONST(SUPRIVATE struct sigutils_codec_class, class);

const struct sigutils_codec_class *
su_codec_class_lookup(const char *name)
{
  unsigned int i;

  for (i = 0; i < class_count; ++i)
    if (strcmp(class_list[i]->name, name) == 0)
      return class_list[i];

  return NULL;
}

SUBOOL
su_codec_class_register(const struct sigutils_codec_class *class)
{
  SU_TRYCATCH(class->name   != NULL, return SU_FALSE);
  SU_TRYCATCH(class->ctor   != NULL, return SU_FALSE);
  SU_TRYCATCH(class->encode != NULL, return SU_FALSE);
  SU_TRYCATCH(class->decode != NULL, return SU_FALSE);
  SU_TRYCATCH(class->dtor   != NULL, return SU_FALSE);

  SU_TRYCATCH(su_codec_class_lookup(class->name) == NULL, return SU_FALSE);
  SU_TRYCATCH(
      PTR_LIST_APPEND_CHECK(class, (void *) class) != -1,
      return SU_FALSE);

  return SU_TRUE;
}

void
su_codec_destroy(su_codec_t *codec)
{
  if (codec->classptr != NULL)
    (codec->classptr->dtor)(codec->privdata);

  free(codec);
}

void
su_codec_set_direction(su_codec_t *codec, enum su_codec_direction dir)
{
  codec->direction = dir;
}

SUSYMBOL
su_codec_feed(su_codec_t *codec, SUSYMBOL x)
{
  switch (codec->direction) {
    case SU_CODEC_DIRECTION_FORWARDS:
      return (codec->classptr->encode) (codec, codec->privdata, x);
    case SU_CODEC_DIRECTION_BACKWARDS:
      return (codec->classptr->decode) (codec, codec->privdata, x);
  }

  return SU_NOSYMBOL;
}

unsigned int
su_codec_get_output_bits(const su_codec_t *codec)
{
  return codec->output_bits;
}

su_codec_t *
su_codec_new(const char *classname, unsigned int bits, ...)
{
  su_codec_t *new = NULL;
  va_list ap;

  va_start(ap, bits);

  SU_TRYCATCH(new = calloc(1, sizeof(su_codec_t)), goto fail);

  new->direction = SU_CODEC_DIRECTION_FORWARDS;
  new->bits = bits;
  new->output_bits = bits; /* Can be modified by ctor */

  if ((new->classptr = su_codec_class_lookup(classname)) == NULL) {
    SU_ERROR("No such codec class `%s'\n", classname);
    goto fail;
  }

  if (!(new->classptr->ctor) (new, &new->privdata, ap)) {
    SU_ERROR("Failed to construct `%s'\n", classname);
    goto fail;
  }

  va_end(ap);

  return new;

fail:
  if (new != NULL)
    su_codec_destroy(new);

  return NULL;
}
