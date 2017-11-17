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

#ifndef _SIGUTILS_ENCODER_H
#define _SIGUTILS_ENCODER_H

#include <stdarg.h>

#include "types.h"

struct sigutils_encoder;

struct sigutils_encoder_class {
  const char *name;
  SUBOOL   (*ctor) (struct sigutils_encoder *, void **, va_list);
  SUSYMBOL (*encode) (struct sigutils_encoder *, void *, SUSYMBOL);
  SUSYMBOL (*decode) (struct sigutils_encoder *, void *, SUSYMBOL);
  void     (*dtor) (void *);
};

enum su_encoder_direction {
  SU_ENCODER_DIRECTION_FORWARDS,
  SU_ENCODER_DIRECTION_BACKWARDS
};

struct sigutils_encoder {
  enum su_encoder_direction direction;
  const struct sigutils_encoder_class *class;
  unsigned int bits;
  void *private;
};

typedef struct sigutils_encoder su_encoder_t;

SUBOOL su_encoder_class_register(const struct sigutils_encoder_class *class);

su_encoder_t *su_encoder_new(const char *classname, unsigned int bits, ...);

void su_encoder_set_direction(
    su_encoder_t *encoder,
    enum su_encoder_direction dir);

SUSYMBOL su_encoder_feed(su_encoder_t *encoder, SUSYMBOL x);

void su_encoder_destroy(su_encoder_t *encoder);

/* Built-in encoders */
SUBOOL su_diff_encoder_register(void);

#endif /* _SIGUTILS_ENCODER_H */
