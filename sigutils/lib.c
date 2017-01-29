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

/* Block classes */
extern struct sigutils_block_class su_block_class_AGC;
extern struct sigutils_block_class su_block_class_TUNER;
extern struct sigutils_block_class su_block_class_WAVFILE;
extern struct sigutils_block_class su_block_class_COSTAS;
extern struct sigutils_block_class su_block_class_RRC;
extern struct sigutils_block_class su_block_class_CDR;
extern struct sigutils_block_class su_block_class_SIGGEN;

/* Modem classes */
extern struct sigutils_modem_class su_modem_class_QPSK;

SUBOOL
su_lib_init(void)
{
  unsigned int i = 0;

  struct sigutils_block_class *blocks[] =
      {
          &su_block_class_AGC,
          &su_block_class_TUNER,
          &su_block_class_WAVFILE,
          &su_block_class_COSTAS,
          &su_block_class_RRC,
          &su_block_class_CDR,
          &su_block_class_SIGGEN,
      };

  struct sigutils_modem_class *modems[] =
      {
          &su_modem_class_QPSK
      };

  for (i = 0; i < sizeof (blocks) / sizeof (blocks[0]); ++i)
    if (!su_block_class_register(blocks[i])) {
      if (blocks[i]->name != NULL)
        SU_ERROR("Failed to register block class `%s'\n", blocks[i]->name);
      return SU_FALSE;
    }

  for (i = 0; i < sizeof (modems) / sizeof (modems[0]); ++i)
    if (!su_modem_class_register(modems[i])) {
      if (modems[i]->name != NULL)
        SU_ERROR("Failed to register modem class `%s'\n", blocks[i]->name);
      return SU_FALSE;
    }

  return SU_TRUE;
}

