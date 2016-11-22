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

#include "sigutils.h"

extern struct sigutils_block_class su_block_class_AGC;
extern struct sigutils_block_class su_block_class_TUNER;
extern struct sigutils_block_class su_block_class_WAVFILE;
extern struct sigutils_block_class su_block_class_COSTAS;

SUBOOL
su_lib_init(void)
{
  unsigned int i = 0;

  struct sigutils_block_class *classes[] =
      {
          &su_block_class_AGC,
          &su_block_class_TUNER,
          &su_block_class_WAVFILE,
          &su_block_class_COSTAS
      };

  for (i = 0; i < sizeof (classes) / sizeof (classes[0]); ++i)
    if (!su_block_class_register(classes[i])) {
      if (classes[i]->name != NULL)
        SU_ERROR("Failed to register class `%s'\n", classes[i]->name);
      return SU_FALSE;
    }

  return SU_TRUE;
}

