/*

  Copyright (C) 2020 Gonzalo José Carracedo Carballal

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

#include <sigutils/matfile.h>

#include "test_list.h"
#include "test_param.h"

SUBOOL
su_test_mat_file_regular(su_test_context_t *ctx)
{
  su_mat_file_t *mf = NULL;
  su_mat_matrix_t *mtx = NULL;

  SUBOOL ok = SU_FALSE;

  SU_TEST_START(ctx);

  SU_TRYCATCH(mf = su_mat_file_new(), goto done);
  SU_TRYCATCH(mtx = su_mat_file_make_matrix(mf, "U", 3, 3), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 0., 0.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 2., 0.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 2., 3.), goto done);

  SU_TRYCATCH(mtx = su_mat_file_make_matrix(mf, "L", 3, 0), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 1., 1.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 2., 2., 0.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 3., 0., 0.), goto done);

  SU_TRYCATCH(su_mat_file_dump(mf, "regular.mat"), goto done);

  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (mf != NULL)
    su_mat_file_destroy(mf);

  return ok;
}

SUBOOL
su_test_mat_file_streaming(su_test_context_t *ctx)
{
  su_mat_file_t *mf = NULL;
  su_mat_matrix_t *mtx = NULL;

  SUBOOL ok = SU_FALSE;

  SU_TEST_START(ctx);

  SU_TRYCATCH(mf = su_mat_file_new(), goto done);
  SU_TRYCATCH(mtx = su_mat_file_make_matrix(mf, "U", 3, 3), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 0., 0.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 2., 0.), goto done);
  SU_TRYCATCH(su_mat_matrix_write_col(mtx, 1., 2., 3.), goto done);

  SU_TRYCATCH(su_mat_file_dump(mf, "streaming.mat"), goto done);

  SU_TRYCATCH(
      mtx = su_mat_file_make_streaming_matrix(mf, "H1", 3, 0),
      goto done);
  SU_TRYCATCH(su_mat_file_stream_col(mf, 1., 1., 1.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);
  SU_TRYCATCH(su_mat_file_stream_col(mf, 2., 2., 2.), goto done);
  SU_TRYCATCH(su_mat_file_stream_col(mf, 3., 3., 3.), goto done);

  SU_TRYCATCH(su_mat_file_flush(mf), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TRYCATCH(
      mtx = su_mat_file_make_streaming_matrix(mf, "H1Long", 3, 0),
      goto done);
  SU_TRYCATCH(su_mat_file_stream_col(mf, 1., 2., 3.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TRYCATCH(su_mat_file_stream_col(mf, 4., 5., 6.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TRYCATCH(su_mat_file_stream_col(mf, 7., 8., 9.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TRYCATCH(su_mat_file_stream_col(mf, 10., 11., 12.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TRYCATCH(su_mat_file_stream_col(mf, 13., 14., 15.), goto done);
  SU_TRYCATCH(su_mat_file_flush(mf), goto done);

  SU_TEST_ASSERT(mtx->cols_alloc == 1);

  SU_TRYCATCH(su_mat_file_flush(mf), goto done);
  ok = SU_TRUE;

done:
  SU_TEST_END(ctx);

  if (mf != NULL)
    su_mat_file_destroy(mf);

  return ok;
}
