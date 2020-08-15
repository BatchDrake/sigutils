/*

  Copyright (C) 2020 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_MATFILE_H
#define _SIGUTILS_MATFILE_H

#include "types.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_mat_matrix {
  char *name;
  int cols;
  int rows;

  int cols_alloc;
  int rows_alloc;

  int col_ptr; /* Keep col pointer in order to populate matrix */

  SUFLOAT **coef;
};

typedef struct sigutils_mat_matrix su_mat_matrix_t;

su_mat_matrix_t *su_mat_matrix_new(const char *name, int rows, int cols);

SUBOOL su_mat_matrix_write_col_va(su_mat_matrix_t *, va_list);
SUBOOL su_mat_matrix_write_col(su_mat_matrix_t *, ...);
SUBOOL su_mat_matrix_write_col_array(su_mat_matrix_t *, const SUFLOAT *);
SUBOOL su_mat_matrix_set_col_ptr(su_mat_matrix_t *, int);
SUBOOL su_mat_matrix_resize(su_mat_matrix_t *, int, int);

void su_mat_matrix_destroy(su_mat_matrix_t *);

struct sigutils_mat_file {
  PTR_LIST(su_mat_matrix_t, matrix);

  FILE *fp;
  su_mat_matrix_t *sm;

  SUSCOUNT sm_off;
  SUSCOUNT sm_contents_off;
  SUSCOUNT sm_last_col;
};

typedef struct sigutils_mat_file su_mat_file_t;

su_mat_file_t *su_mat_file_new(void);

su_mat_matrix_t *su_mat_file_lookup_matrix(
    const su_mat_file_t *,
    const char *);

SUBOOL su_mat_file_give_matrix(su_mat_file_t *, su_mat_matrix_t *);
SUBOOL su_mat_file_give_streaming_matrix(
    su_mat_file_t *,
    su_mat_matrix_t *);

su_mat_matrix_t *su_mat_file_make_matrix(
    su_mat_file_t *self,
    const char *name,
    int cols,
    int rows);

su_mat_matrix_t *su_mat_file_make_streaming_matrix(
    su_mat_file_t *self,
    const char *name,
    int cols,
    int rows);

SUBOOL su_mat_file_stream_col(su_mat_file_t *, ...);

SUBOOL su_mat_file_dump(su_mat_file_t *, const char *);
SUBOOL su_mat_file_flush(su_mat_file_t *);
void su_mat_file_destroy(su_mat_file_t *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_MATFILE_H */
