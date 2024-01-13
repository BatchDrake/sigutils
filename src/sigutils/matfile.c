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

#define SU_LOG_DOMAIN "matfile"

#include <sigutils/matfile.h>

#include <stdarg.h>
#include <string.h>

#include <sigutils/log.h>
#include <sigutils/version.h>

#define SU_MAT_FILE_VERSION 0x100
#define SU_MAT_FILE_ENDIANNESS (('M' << 8) | ('I'))
#define SU_MAT_FILE_ALIGNMENT 8

#define SU_MAT_ALIGN(x) __ALIGN(x, SU_MAT_FILE_ALIGNMENT)
#define SU_MAT_miINT8 1
#define SU_MAT_miUINT8 2
#define SU_MAT_miINT16 3
#define SU_MAT_miUINT16 4
#define SU_MAT_miINT32 5
#define SU_MAT_miUINT32 6
#define SU_MAT_miSINGLE 7
#define SU_MAT_miDOUBLE 9
#define SU_MAT_miINT64 12
#define SU_MAT_miUINT64 13
#define SU_MAT_miMATRIX 14

#define SU_MAT_mxSINGLE_CLASS 7

struct sigutils_mat_header {
  char description[124];
  uint16_t version;
  uint16_t endianness;
} __attribute__((packed));

struct sigutils_mat_tag {
  union {
    struct {
      uint16_t type;
      uint16_t size;
      uint8_t data[4];
    } small;

    struct {
      uint32_t type;
      uint32_t size;
      uint8_t data[0];
    } big;
  };
} __attribute__((packed));

SUPRIVATE SUBOOL
su_mat_file_dump_matrix(su_mat_file_t *self, const su_mat_matrix_t *matrix);

SUBOOL
su_mat_matrix_resize(su_mat_matrix_t *self, int rows, int cols)
{
  int curr_alloc = self->cols_alloc;
  int true_cols;
  int i;

  SUFLOAT **tmp;

  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(rows <= self->rows_alloc, goto done);
  SU_TRYCATCH(self->col_start <= cols, goto done);

  self->rows = rows;
  true_cols = cols - self->col_start;

  if (true_cols > curr_alloc) {
    if (curr_alloc == 0)
      curr_alloc = true_cols;
    else
      while (curr_alloc < true_cols)
        curr_alloc <<= 1;

    SU_TRYCATCH(
        tmp = realloc(self->coef, sizeof(SUFLOAT *) * curr_alloc),
        goto done);

    memset(
        tmp + self->cols_alloc,
        0,
        sizeof(SUFLOAT *) * (curr_alloc - self->cols_alloc));

    self->coef = tmp;

    for (i = self->cols_alloc; i < curr_alloc; ++i) {
      SU_TRYCATCH(
          self->coef[i] = calloc(self->rows_alloc, sizeof(SUFLOAT)),
          goto done);
      ++self->cols_alloc;
    }
  }

  self->cols = true_cols;

  ok = SU_TRUE;

done:
  return ok;
}

su_mat_matrix_t *
su_mat_matrix_new(const char *name, int rows, int cols)
{
  su_mat_matrix_t *new = NULL;

  SU_TRYCATCH(rows > 0, goto fail);
  SU_TRYCATCH(new = calloc(1, sizeof(su_mat_matrix_t)), goto fail);
  SU_TRYCATCH(new->name = strdup(name), goto fail);

  new->cols = cols;
  new->rows = rows;

  new->rows_alloc = rows;

  SU_TRYCATCH(su_mat_matrix_resize(new, rows, cols), goto fail);

  return new;

fail:
  if (new != NULL)
    su_mat_matrix_destroy(new);

  return NULL;
}

SUBOOL
su_mat_matrix_set_col_ptr(su_mat_matrix_t *self, int ptr)
{
  SU_TRYCATCH(ptr >= 0, return SU_FALSE);

  self->col_ptr = ptr;

  return SU_TRUE;
}

void
su_mat_matrix_discard_cols(su_mat_matrix_t *self)
{
  self->col_start += self->cols;
  self->cols = 0;
  self->col_ptr = 0;
}

SUBOOL
su_mat_matrix_write_col_va(su_mat_matrix_t *self, va_list ap)
{
  int i, rows = self->rows;
  int ptr = self->col_ptr;
  SUBOOL ok = SU_FALSE;

  if (ptr >= self->cols)
    SU_TRYCATCH(
        su_mat_matrix_resize(self, self->rows, self->col_start + ptr + 1),
        goto done);

  /*
   * I know this looks odd. It's actually C ABI's fault: all floats get
   * automatically promoted to double when passed via vararg
   */

  for (i = 0; i < rows; ++i)
    self->coef[ptr][i] = SU_ASFLOAT(va_arg(ap, double));

  self->col_ptr = ptr + 1;

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_mat_matrix_write_col(su_mat_matrix_t *self, ...)
{
  va_list ap;
  SUBOOL ok = SU_FALSE;

  va_start(ap, self);

  ok = su_mat_matrix_write_col_va(self, ap);

  va_end(ap);

  return ok;
}

SUBOOL
su_mat_matrix_write_col_array(su_mat_matrix_t *self, const SUFLOAT *x)
{
  int ptr = self->col_ptr;

  if (ptr >= self->cols)
    SU_TRYCATCH(
        su_mat_matrix_resize(self, self->rows, ptr + 1),
        return SU_FALSE);

  memcpy(self->coef[ptr], x, sizeof(SUFLOAT) * self->rows);

  self->col_ptr = ptr + 1;

  return SU_TRUE;
}

void
su_mat_matrix_destroy(su_mat_matrix_t *self)
{
  int i;

  if (self->name != NULL)
    free(self->name);

  for (i = 0; i < self->cols_alloc; ++i)
    if (self->coef[i] != NULL)
      free(self->coef[i]);

  if (self->coef != NULL)
    free(self->coef);

  free(self);
}

su_mat_file_t *
su_mat_file_new(void)
{
  su_mat_file_t *new;

  SU_TRYCATCH(new = calloc(1, sizeof(su_mat_file_t)), goto fail);

  return new;

fail:
  if (new != NULL)
    su_mat_file_destroy(new);

  return NULL;
}

int
su_mat_file_lookup_matrix_handle(const su_mat_file_t *self, const char *name)
{
  unsigned int i;

  for (i = 0; i < self->matrix_count; ++i)
    if (self->matrix_list[i] != NULL
        && strcmp(self->matrix_list[i]->name, name) == 0)
      return (int)i;

  return -1;
}

su_mat_matrix_t *
su_mat_file_get_matrix_by_handle(const su_mat_file_t *self, int handle)
{
  if (handle < 0 || handle >= (int)self->matrix_count)
    return NULL;

  return self->matrix_list[handle];
}

su_mat_matrix_t *
su_mat_file_lookup_matrix(const su_mat_file_t *self, const char *name)
{
  return su_mat_file_get_matrix_by_handle(
      self,
      su_mat_file_lookup_matrix_handle(self, name));
}

SUBOOL
su_mat_file_give_matrix(su_mat_file_t *self, su_mat_matrix_t *mat)
{
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(su_mat_file_lookup_matrix(self, mat->name) == NULL, goto done);

  SU_TRYCATCH(PTR_LIST_APPEND_CHECK(self->matrix, mat) != -1, goto done);

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_mat_file_give_streaming_matrix(su_mat_file_t *self, su_mat_matrix_t *mat)
{
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(su_mat_file_lookup_matrix(self, mat->name) == NULL, goto done);

  if (self->fp != NULL && self->sm != NULL)
    SU_TRYCATCH(su_mat_file_flush(self), goto done);

  SU_TRYCATCH(PTR_LIST_APPEND_CHECK(self->matrix, mat) != -1, goto done);

  self->sm = mat;
  self->sm_last_col = 0;

  if (self->fp != NULL)
    SU_TRYCATCH(su_mat_file_dump_matrix(self, mat), goto done);

  ok = SU_TRUE;

done:
  return ok;
}

su_mat_matrix_t *
su_mat_file_make_matrix(
    su_mat_file_t *self,
    const char *name,
    int rows,
    int cols)
{
  su_mat_matrix_t *new = NULL;

  SU_TRYCATCH(new = su_mat_matrix_new(name, rows, cols), goto fail);

  SU_TRYCATCH(su_mat_file_give_matrix(self, new), goto fail);

  return new;

fail:
  if (new != NULL)
    su_mat_matrix_destroy(new);

  return NULL;
}

SUBOOL
su_mat_file_stream_col(su_mat_file_t *self, ...)
{
  va_list ap;
  SUBOOL ok = SU_FALSE;

  SU_TRYCATCH(self->sm != NULL, return SU_FALSE);

  va_start(ap, self);

  ok = su_mat_matrix_write_col_va(self->sm, ap);

  va_end(ap);

  return ok;
}

su_mat_matrix_t *
su_mat_file_make_streaming_matrix(
    su_mat_file_t *self,
    const char *name,
    int rows,
    int cols)
{
  su_mat_matrix_t *new = NULL;

  SU_TRYCATCH(new = su_mat_matrix_new(name, rows, cols), goto fail);

  SU_TRYCATCH(su_mat_file_give_streaming_matrix(self, new), goto fail);

  return new;

fail:
  if (new != NULL)
    su_mat_matrix_destroy(new);

  return NULL;
}

SUPRIVATE SUBOOL
su_mat_file_write_big_tag(su_mat_file_t *self, uint32_t type, uint32_t size)
{
  struct sigutils_mat_tag tag;
  tag.big.type = type;
  tag.big.size = size;

  return fwrite(&tag, sizeof(struct sigutils_mat_tag), 1, self->fp) == 1;
}

SUPRIVATE SUBOOL
su_mat_file_write_small_tag(
    su_mat_file_t *self,
    uint32_t type,
    uint32_t size,
    const void *data)
{
  struct sigutils_mat_tag tag;

  memset(&tag, 0, sizeof(struct sigutils_mat_tag));

  tag.small.type = type;
  tag.small.size = size;

  if (size > 4)
    size = 4;

  memcpy(tag.small.data, data, size);

  return fwrite(&tag, sizeof(struct sigutils_mat_tag), 1, self->fp) == 1;
}

SUPRIVATE SUBOOL
su_mat_file_write_tag(
    su_mat_file_t *self,
    uint32_t type,
    uint32_t size,
    const void *data)
{
  uint32_t align_pad_size = SU_MAT_ALIGN(size) - size;
  uint64_t align_pad_data = 0;

  if (size <= 4) {
    return su_mat_file_write_small_tag(self, type, size, data);
  } else {
    SU_TRYCATCH(su_mat_file_write_big_tag(self, type, size), return SU_FALSE);

    SU_TRYCATCH(fwrite(data, size, 1, self->fp) == 1, return SU_FALSE);

    SU_TRYCATCH(
        fwrite(&align_pad_data, align_pad_size, 1, self->fp) == 1,
        return SU_FALSE);
  }

  return SU_TRUE;
}

SUPRIVATE SUBOOL
su_mat_file_write_uint32(su_mat_file_t *self, uint32_t val)
{
  return fwrite(&val, sizeof(uint32_t), 1, self->fp);
}

SUPRIVATE SUBOOL
su_mat_file_write_int32(su_mat_file_t *self, int32_t val)
{
  return fwrite(&val, sizeof(int32_t), 1, self->fp);
}

SUPRIVATE SUBOOL
su_mat_file_dump_matrix(su_mat_file_t *self, const su_mat_matrix_t *matrix)
{
  uint32_t metadata_size = sizeof(struct sigutils_mat_tag) * 6;
  uint32_t matrix_size = SU_MAT_ALIGN(
      sizeof(SUFLOAT) * matrix->rows * (matrix->cols + matrix->col_start));
  off_t last_off;
  uint32_t extra_size;
  uint64_t pad = 0;
  SUFLOAT gap = 0;
  int i;
  SUBOOL ok = SU_FALSE;

  if (strlen(matrix->name) > 4)
    metadata_size += SU_MAT_ALIGN(strlen(matrix->name));

  fseek(self->fp, SU_MAT_ALIGN(ftell(self->fp)), SEEK_SET);

  /* Matrix header */
  if (matrix == self->sm)
    self->sm_off = ftell(self->fp);
  SU_TRYCATCH(
      su_mat_file_write_big_tag(
          self,
          SU_MAT_miMATRIX,
          metadata_size + matrix_size),
      goto done);

  /* Array flags */
  SU_TRYCATCH(
      su_mat_file_write_big_tag(self, SU_MAT_miUINT32, 2 * sizeof(uint32_t)),
      goto done);
  SU_TRYCATCH(su_mat_file_write_uint32(self, SU_MAT_mxSINGLE_CLASS), goto done);
  SU_TRYCATCH(su_mat_file_write_uint32(self, 0), goto done);

  /* Dimensions array */
  SU_TRYCATCH(
      su_mat_file_write_big_tag(self, SU_MAT_miINT32, 2 * sizeof(int32_t)),
      goto done);
  SU_TRYCATCH(su_mat_file_write_int32(self, matrix->rows), goto done);
  SU_TRYCATCH(
      su_mat_file_write_int32(self, matrix->cols + matrix->col_start),
      goto done);

  /* Name */
  SU_TRYCATCH(
      su_mat_file_write_tag(
          self,
          SU_MAT_miINT8,
          strlen(matrix->name),
          matrix->name),
      goto done);

  /* Data */
  if (matrix == self->sm)
    self->sm_contents_off = ftell(self->fp);
  SU_TRYCATCH(
      su_mat_file_write_big_tag(self, SU_MAT_miSINGLE, matrix_size),
      goto done);

  for (i = 0; i < matrix->col_start * matrix->rows; ++i)
    SU_TRYCATCH(fwrite(&gap, sizeof(SUFLOAT), 1, self->fp) == 1, goto done);

  for (i = 0; i < matrix->cols; ++i)
    SU_TRYCATCH(
        fwrite(matrix->coef[i], sizeof(SUFLOAT) * matrix->rows, 1, self->fp)
            == 1,
        goto done);

  if (matrix == self->sm)
    self->sm_last_col = matrix->cols + matrix->col_start;

  /* Correct file alignment */
  last_off = ftell(self->fp);
  extra_size = SU_MAT_ALIGN(last_off) - last_off;

  if (extra_size > 0) {
    SU_TRYCATCH(fwrite(&pad, extra_size, 1, self->fp) == 1, goto done);

    /* Streaming matrices are not considered "done" */
    if (matrix == self->sm)
      SU_TRYCATCH(fseek(self->fp, last_off, SEEK_SET) != -1, goto done);
  }

  fflush(stdout);

  ok = SU_TRUE;

done:
  return ok;
}

SUBOOL
su_mat_file_dump(su_mat_file_t *self, const char *path)
{
  FILE *fp = NULL;
  unsigned int i;
  SUBOOL ok = SU_FALSE;

  struct sigutils_mat_header header;

  fp = fopen(path, "w+b");

  if (fp == NULL) {
    SU_ERROR("Failed to open `%s' for writing: %s\n", path, strerror(errno));
    goto done;
  }

  memset(&header, 0, sizeof(struct sigutils_mat_header));
  memset(header.description, ' ', sizeof(header.description));

  strcpy(
      header.description,
      "MATLAB 5.0 MAT-file, written by Sigutils " SIGUTILS_VERSION_STRING);

  header.version = SU_MAT_FILE_VERSION;
  header.endianness = SU_MAT_FILE_ENDIANNESS;

  SU_TRYCATCH(
      fwrite(&header, sizeof(struct sigutils_mat_header), 1, fp) == 1,
      goto done);

  if (self->fp != NULL)
    fclose(self->fp);

  self->fp = fp;

  for (i = 0; i < self->matrix_count; ++i)
    if (self->matrix_list[i] != NULL && self->matrix_list[i] != self->sm)
      SU_TRYCATCH(
          su_mat_file_dump_matrix(self, self->matrix_list[i]),
          goto done);

  if (self->sm != NULL)
    SU_TRYCATCH(su_mat_file_dump_matrix(self, self->sm), goto done);

  fp = NULL;

  ok = SU_TRUE;

done:
  if (fp != NULL)
    fclose(fp);

  return ok;
}

SUBOOL
su_mat_file_flush(su_mat_file_t *self)
{
  SUBOOL ok = SU_FALSE;
  int i;
  off_t last_off;
  uint32_t extra_size;
  uint64_t pad = 0;
  uint32_t metadata_size = sizeof(struct sigutils_mat_tag) * 6;
  uint32_t matrix_size;
  uint64_t total_cols;

  SU_TRYCATCH(self->fp != NULL, goto done);

  if (self->sm != NULL) {
    total_cols = self->sm->cols + self->sm->col_start;
    matrix_size = SU_MAT_ALIGN(sizeof(SUFLOAT) * self->sm->rows * total_cols);

    if (strlen(self->sm->name) > 4)
      metadata_size += SU_MAT_ALIGN(strlen(self->sm->name));

    last_off = ftell(self->fp);
    if (self->sm_last_col < total_cols) {
      extra_size =
          (total_cols - self->sm_last_col) * self->sm->rows * sizeof(SUFLOAT);

      /* Increment matrix size */
      SU_TRYCATCH(fseek(self->fp, self->sm_off, SEEK_SET) != -1, goto done);
      SU_TRYCATCH(
          su_mat_file_write_big_tag(
              self,
              SU_MAT_miMATRIX,
              metadata_size + matrix_size),
          goto done);

      /* Refresh matrix dimensions */
      SU_TRYCATCH(
          fseek(self->fp, self->sm_off + 32, SEEK_SET) != -1,
          goto done);
      SU_TRYCATCH(su_mat_file_write_int32(self, self->sm->rows), goto done);
      SU_TRYCATCH(su_mat_file_write_int32(self, total_cols), goto done);

      /* Increment matrix data size */
      SU_TRYCATCH(
          fseek(self->fp, self->sm_contents_off, SEEK_SET) != -1,
          goto done);
      SU_TRYCATCH(
          su_mat_file_write_big_tag(self, SU_MAT_miSINGLE, matrix_size),
          goto done);

      SU_TRYCATCH(fseek(self->fp, last_off, SEEK_SET) != -1, goto done);

      for (i = self->sm_last_col - self->sm->col_start; i < self->sm->cols; ++i)
        SU_TRYCATCH(
            fwrite(
                self->sm->coef[i],
                sizeof(SUFLOAT) * self->sm->rows,
                1,
                self->fp)
                == 1,
            goto done);

      self->sm_last_col = total_cols;
      su_mat_matrix_discard_cols(self->sm);

      /* Correct file alignment */
      last_off = ftell(self->fp);
      extra_size = SU_MAT_ALIGN(last_off) - last_off;

      if (extra_size > 0) {
        SU_TRYCATCH(fwrite(&pad, extra_size, 1, self->fp) == 1, goto done);
        SU_TRYCATCH(fseek(self->fp, last_off, SEEK_SET) != -1, goto done);
      }

      fflush(self->fp);
    }
  }

  ok = SU_TRUE;

done:
  return ok;
}

void
su_mat_file_destroy(su_mat_file_t *self)
{
  unsigned int i;

  if (self->fp != NULL)
    fclose(self->fp);

  for (i = 0; i < self->matrix_count; ++i)
    if (self->matrix_list[i] != NULL)
      su_mat_matrix_destroy(self->matrix_list[i]);

  if (self->matrix_list != NULL)
    free(self->matrix_list);

  free(self);
}
