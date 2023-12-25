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

#include <sigutils/block.h>
#include <sigutils/log.h>
#include <sigutils/types.h>
#include <sigutils/version.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Library initialization */
SUBOOL su_lib_init_ex(const struct sigutils_log_config *logconfig);
SUBOOL su_lib_init(void);

/* FFT wisdom file handling */
SUBOOL su_lib_is_using_wisdom(void);
SUBOOL su_lib_set_wisdom_enabled(SUBOOL);
SUBOOL su_lib_set_wisdom_file(const char *);
SUBOOL su_lib_save_wisdom(void);
void   su_lib_gen_wisdom(void);

/* Internal */
int su_lib_fftw_strategy(void);
SU_FFTW(_plan) su_lib_plan_dft_1d(int n, SU_FFTW(_complex) *in,
        SU_FFTW(_complex) *out, int sign, unsigned flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_SIGUTILS_H */
