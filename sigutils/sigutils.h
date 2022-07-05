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

#ifndef _SIGUTILS_SIGUTILS_H
#define _SIGUTILS_SIGUTILS_H

#include "block.h"
#include "log.h"
#include "types.h"
#include "version.h"

SUBOOL su_lib_init_ex(const struct sigutils_log_config *logconfig);
SUBOOL su_lib_init(void);

#endif /* _SIGUTILS_SIGUTILS_H */
