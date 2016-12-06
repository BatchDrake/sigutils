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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>
#include "test.h"

SUBOOL
su_sigbuf_pool_helper_dump_wav(
    const void *data,
    SUSCOUNT size,
    SUSCOUNT fs,
    SUBOOL is_complex,
    const char *directory,
    const char *name)
{
  SNDFILE *sf = NULL;
  char *filename = NULL;
  SF_INFO info;
  SUSCOUNT samples = 0;
  SUSCOUNT written = 0;
  unsigned int i;

  info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
  info.channels = is_complex ? 2 : 1;
  info.samplerate = fs;

  filename = strbuild("%s/%s.wav", directory, name);
  if (filename == NULL) {
    SU_ERROR("Memory error while building filename\n");
    goto fail;
  }

  if ((sf = sf_open(filename, SFM_WRITE, &info)) == NULL) {
    SU_ERROR("Cannot open `%s' for write: %s\n", filename, sf_strerror(sf));
    goto fail;
  }

  samples = size * info.channels;

  /* UGLY HACK: WE ASSUME THAT A COMPLEX IS TWO DOUBLES */
  if ((written = sf_write_double(sf, (const SUFLOAT *) data, samples))
      != samples) {
    SU_ERROR("Write to `%s' failed: %lu/%lu\n", filename, written, samples);
    goto fail;
  }

  free(filename);
  sf_close(sf);

  return SU_TRUE;

fail:
  if (filename != NULL)
    free(filename);

  if (sf != NULL)
    sf_close(sf);

  return SU_FALSE;
}

SUBOOL
su_sigbuf_pool_dump_wav(const su_sigbuf_pool_t *pool)
{
  su_sigbuf_t *this;
  unsigned int i;

  FOR_EACH_PTR(this, i, pool->sigbuf) {
    if (!su_sigbuf_pool_helper_dump_wav(
        this->buffer,
        this->size,
        this->fs,
        this->is_complex,
        pool->name,
        this->name))
      return SU_FALSE;
  }

  return SU_TRUE;
}


