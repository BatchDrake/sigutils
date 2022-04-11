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
#include <sndfile.h>
#include <stdlib.h>

#define SU_LOG_LEVEL "wavfile-block"

#include "block.h"
#include "log.h"

#ifdef _SU_SINGLE_PRECISION
#  define sf_read sf_read_float
#else
#  define sf_read sf_read_double
#endif

struct su_wavfile {
  SF_INFO info;
  SNDFILE *sf;
  uint64_t samp_rate;
  SUFLOAT *buffer;
  SUSCOUNT
  size; /* Number of samples PER CHANNEL, buffer size is size * chans */
};

SUPRIVATE void
su_wavfile_close(struct su_wavfile *wav)
{
  if (wav->sf != NULL)
    sf_close(wav->sf);

  if (wav->buffer != NULL)
    free(wav->buffer);

  free(wav);
}

SUPRIVATE struct su_wavfile *
su_wavfile_open(const char *path)
{
  struct su_wavfile *wav = NULL;
  SUBOOL ok = SU_FALSE;

  if ((wav = calloc(1, sizeof(struct su_wavfile))) == NULL) {
    SU_ERROR("Cannot allocate su_wav\n");
    goto done;
  }

  if ((wav->sf = sf_open(path, SFM_READ, &wav->info)) == NULL) {
    SU_ERROR("Cannot open `%s': %s\n", path, sf_strerror(wav->sf));
    goto done;
  }

  if (wav->info.channels > 2) {
    SU_ERROR(
        "Cannot open `%s': too many channels (%d)\n",
        path,
        wav->info.channels);
    goto done;
  }

  wav->size = SU_BLOCK_STREAM_BUFFER_SIZE;
  if ((wav->buffer = malloc(wav->size * wav->info.channels * sizeof(SUFLOAT)))
      == NULL) {
    SU_ERROR("Cannot allocate sample buffer\n");
    goto done;
  }

  ok = SU_TRUE;

done:
  if (!ok && wav != NULL) {
    su_wavfile_close(wav);
    wav = NULL;
  }

  return wav;
}

SUPRIVATE SUBOOL
su_block_wavfile_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  struct su_wavfile *wav = NULL;
  const char *path = NULL;

  path = va_arg(ap, const char *);

  if ((wav = su_wavfile_open(path)) == NULL) {
    SU_ERROR("Constructor failed\n");
    return SU_FALSE;
  }

  wav->samp_rate = wav->info.samplerate;

  if (!su_block_set_property_ref(
          block,
          SU_PROPERTY_TYPE_INTEGER,
          "samp_rate",
          &wav->samp_rate)
      || !su_block_set_property_ref(
          block,
          SU_PROPERTY_TYPE_INTEGER,
          "channels",
          &wav->info.channels)) {
    su_wavfile_close(wav);
    return SU_FALSE;
  }

  *private = (void *)wav;

  return SU_TRUE;
}

SUPRIVATE void
su_block_wavfile_dtor(void *private)
{
  su_wavfile_close((struct su_wavfile *)private);
}

/*
 * We process wav files as I/Q data. Left channel contains I data and
 * right channel contains Q data.
 */
SUPRIVATE SUSDIFF
su_block_wavfile_acquire(
    void *priv,
    su_stream_t *out,
    unsigned int port_id,
    su_block_port_t *in)
{
  struct su_wavfile *wav = (struct su_wavfile *)priv;
  SUSDIFF size;
  SUSDIFF got;
  unsigned int i;
  SUCOMPLEX *start;

  /* Get the number of complex samples to write */
  size = su_stream_get_contiguous(out, &start, SU_MIN(wav->size, out->size));

  if ((got = sf_read(wav->sf, wav->buffer, size * wav->info.channels)) > 0) {
    if (wav->info.channels == 1) {
      /* One channel: assume real data */
      for (i = 0; i < got; ++i)
        start[i] = wav->buffer[i];
    } else {
      /*
       * Stereo: assume complex. Divide got by two, that is the number of
       * complex samples to write.
       */
      got >>= 1;

      for (i = 0; i < got; ++i)
        start[i] = wav->buffer[i << 1] + I * wav->buffer[(i << 1) + 1];
    }

    /* Increment position */
    if (su_stream_advance_contiguous(out, got) != got) {
      SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
      return -1;
    }
  } else if (got < 0) {
    SU_ERROR("Error while reading wav file: %s\n", sf_error(wav->sf));
    return -1;
  }

  return got;
}

struct sigutils_block_class su_block_class_WAVFILE = {
    "wavfile",                /* name */
    0,                        /* in_size */
    1,                        /* out_size */
    su_block_wavfile_ctor,    /* constructor */
    su_block_wavfile_dtor,    /* destructor */
    su_block_wavfile_acquire, /* acquire */
};
