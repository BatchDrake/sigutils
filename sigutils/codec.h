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

#ifndef _SIGUTILS_CODEC_H
#define _SIGUTILS_CODEC_H

#include <stdarg.h>

#include "types.h"

struct sigutils_codec;

struct sigutils_codec_class {
  const char *name;
  SUBOOL   (*ctor) (struct sigutils_codec *, void **, va_list);
  SUSYMBOL (*encode) (struct sigutils_codec *, void *, SUSYMBOL);
  SUSYMBOL (*decode) (struct sigutils_codec *, void *, SUSYMBOL);
  void     (*dtor) (void *);
};

enum su_codec_direction {
  SU_CODEC_DIRECTION_FORWARDS,
  SU_CODEC_DIRECTION_BACKWARDS
};

struct sigutils_codec {
  enum su_codec_direction direction;
  const struct sigutils_codec_class *class;
  unsigned int bits;
  void *private;
};

typedef struct sigutils_codec su_codec_t;

SUBOOL su_codec_class_register(const struct sigutils_codec_class *class);

su_codec_t *su_codec_new(const char *classname, unsigned int bits, ...);

void su_codec_set_direction(
    su_codec_t *codec,
    enum su_codec_direction dir);

SUSYMBOL su_codec_feed(su_codec_t *codec, SUSYMBOL x);

void su_codec_destroy(su_codec_t *codec);

/* Built-in codecs */
SUBOOL su_diff_codec_register(void);

#endif /* _SIGUTILS_CODEC_H */
