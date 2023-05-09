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

#include <sigutils/iir.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sigutils/coef.h>
#include <sigutils/sampling.h>
#include <sigutils/taps.h>

/*
  Somehow the Volk enhancement of IIR filters corrupts the
  heap in Win32 systems. The reason is not well understood.

  We temptatively disable it in these targets until we figure
  out what the heck is going on.
*/

#if !defined(_WIN32) && defined(_SU_SINGLE_PRECISION) && HAVE_VOLK
#  define SU_USE_VOLK
#  include <volk/volk.h>
#endif

#ifdef SU_USE_VOLK
#  define calloc su_volk_calloc
#  define malloc su_volk_malloc
#  define free volk_free
SUINLINE void *
su_volk_malloc(size_t size)
{
  return volk_malloc(size, volk_get_alignment());
}

SUINLINE void *
su_volk_calloc(size_t nmemb, size_t size)
{
  void *result = su_volk_malloc(nmemb * size);

  if (result != NULL)
    memset(result, 0, nmemb * size);

  return result;
}
#endif /* SU_USE_VOLK */

SUINLINE void
__su_iir_filt_push_x(su_iir_filt_t *filt, SUCOMPLEX x)
{
#ifdef SU_USE_VOLK
  if (--filt->x_ptr < 0)
    filt->x_ptr += filt->x_size; /* ptr: size - 1 */
  else
    filt->x[filt->x_ptr + filt->x_size] = x;

  filt->x[filt->x_ptr] = x;
#else
  filt->x[filt->x_ptr++] = x;
  if (filt->x_ptr >= filt->x_size)
    filt->x_ptr = 0;
#endif /* SU_USE_VOLK */
}

SUINLINE void
__su_iir_filt_push_y(su_iir_filt_t *filt, SUCOMPLEX y)
{
  if (filt->y_size > 0) {
#ifdef SU_USE_VOLK
    if (--filt->y_ptr < 0)
      filt->y_ptr += filt->y_size; /* ptr: size - 1 */
    else
      filt->y[filt->y_ptr + filt->y_size] = y;

    filt->y[filt->y_ptr] = y;
#else
    filt->y[filt->y_ptr++] = y;
    if (filt->y_ptr >= filt->y_size)
      filt->y_ptr = 0;
#endif /* SU_USE_VOLK */
  }
}

SUINLINE SUCOMPLEX
__su_iir_filt_eval(const su_iir_filt_t *filt)
{
#ifdef SU_USE_VOLK
  SUCOMPLEX y_tmp = 0;
#else
  unsigned int i;
  int p;
#endif /* SU_USE_VOLK */

  SUCOMPLEX y = 0;

  /* Input feedback */
#ifdef SU_USE_VOLK

  volk_32fc_32f_dot_prod_32fc(&y, filt->x + filt->x_ptr, filt->b, filt->x_size);

#else
  p = filt->x_ptr - 1;
  for (i = 0; i < filt->x_size; ++i) {
    if (p < 0)
      p += filt->x_size;

    y += filt->b[i] * filt->x[p--];
  }
#endif /* SU_USE_VOLK */

  if (filt->y_size > 0) {
#ifdef SU_USE_VOLK
    volk_32fc_32f_dot_prod_32fc(
        &y_tmp,
        filt->y + filt->y_ptr,
        filt->a + 1,
        filt->y_size - 1);

    y -= y_tmp;

#else
    /* Output feedback - assumes that a[0] is 1 */
    p = filt->y_ptr - 1;
    for (i = 1; i < filt->y_size; ++i) {
      if (p < 0)
        p += filt->y_size;

      y -= filt->a[i] * filt->y[p--];
    }
#endif /* SU_USE_VOLK */
  }

  return y;
}

void
su_iir_filt_finalize(su_iir_filt_t *filt)
{
  if (filt->a != NULL)
    free(filt->a);

  if (filt->b != NULL)
    free(filt->b);

  if (filt->x != NULL)
    free(filt->x);

  if (filt->y != NULL)
    free(filt->y);
}

SUCOMPLEX
su_iir_filt_feed(su_iir_filt_t *filt, SUCOMPLEX x)
{
  SUCOMPLEX y;

  __su_iir_filt_push_x(filt, x);
  y = __su_iir_filt_eval(filt);
  __su_iir_filt_push_y(filt, y);

  filt->curr_y = y;

  return filt->gain * y;
}

void
su_iir_filt_feed_bulk(
    su_iir_filt_t *filt,
    const SUCOMPLEX *__restrict x,
    SUCOMPLEX *__restrict y,
    SUSCOUNT len)
{
  SUCOMPLEX tmp_y = 0;

  while (len-- != 0) {
    __su_iir_filt_push_x(filt, *x++);
    tmp_y = __su_iir_filt_eval(filt);
    __su_iir_filt_push_y(filt, tmp_y);
    *y++ = filt->gain * tmp_y;
  }

  filt->curr_y = tmp_y;
}

SUCOMPLEX
su_iir_filt_get(const su_iir_filt_t *filt)
{
  return filt->gain * filt->curr_y;
}

void
su_iir_filt_reset(su_iir_filt_t *filt)
{
  memset(filt->x, 0, sizeof(SUCOMPLEX) * filt->x_alloc);
  memset(filt->y, 0, sizeof(SUCOMPLEX) * filt->y_alloc);
  filt->curr_y = 0;
  filt->x_ptr = 0;
  filt->y_ptr = 0;
}

void
su_iir_filt_set_gain(su_iir_filt_t *filt, SUFLOAT gain)
{
  filt->gain = gain;
}

SUBOOL
__su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    SUFLOAT *__restrict a,
    SUSCOUNT x_size,
    SUFLOAT *__restrict b,
    SUBOOL copy_coef)
{
  SUCOMPLEX *x = NULL;
  SUCOMPLEX *y = NULL;
  SUFLOAT *a_copy = NULL;
  SUFLOAT *b_copy = NULL;
  unsigned int x_alloc = x_size;
  unsigned int y_alloc = y_size;

  assert(x_size > 0);

  memset(filt, 0, sizeof(su_iir_filt_t));

  filt->gain = 1;

#ifdef SU_USE_VOLK
  /*
   * When Volk is enabled, we use contiguous buffers by duplicating
   * values in the X and Y buffers
   */
  x_alloc = 2 * x_alloc - 1;

  if (y_alloc > 0)
    y_alloc = 2 * y_alloc - 1;
#endif /* SU_USE_VOLK */

  if ((x = calloc(x_alloc, sizeof(SUCOMPLEX))) == NULL)
    goto fail;

  if (y_size > 0)
    if ((y = calloc(y_alloc, sizeof(SUCOMPLEX))) == NULL)
      goto fail;

  if (copy_coef) {
    if (y_size > 0) {
      if ((a_copy = malloc(y_size * sizeof(SUFLOAT))) == NULL)
        goto fail;

      memcpy(a_copy, a, y_size * sizeof(SUFLOAT));
    }

    if ((b_copy = malloc(x_size * sizeof(SUFLOAT))) == NULL)
      goto fail;

    memcpy(b_copy, b, x_size * sizeof(SUFLOAT));
  } else {
    a_copy = a;
    b_copy = b;
  }

  filt->x = x;
  filt->y = y;

  filt->a = a_copy;
  filt->b = b_copy;

  filt->x_ptr = 0;
  filt->y_ptr = 0;

  filt->x_size = x_size;
  filt->y_size = y_size;

  filt->x_alloc = x_alloc;
  filt->y_alloc = y_alloc;

  return SU_TRUE;

fail:
  if (x != NULL)
    free(x);

  if (y != NULL)
    free(y);

  if (copy_coef) {
    if (a_copy != NULL)
      free(a_copy);

    if (b_copy != NULL)
      free(b_copy);
  }

  return SU_FALSE;
}

SUBOOL
su_iir_filt_init(
    su_iir_filt_t *filt,
    SUSCOUNT y_size,
    const SUFLOAT *__restrict a,
    SUSCOUNT x_size,
    const SUFLOAT *__restrict b)
{
  return __su_iir_filt_init(
      filt,
      y_size,
      (SUFLOAT *)a,
      x_size,
      (SUFLOAT *)b,
      SU_TRUE);
}

SUBOOL
su_iir_bwlpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc)
{
  SUFLOAT *a = NULL;
  SUFLOAT *b = NULL;
  SUFLOAT scaling;

  unsigned int i;

  if ((a = su_dcof_bwlp(n, fc)) == NULL)
    goto fail;

  if ((b = su_ccof_bwlp(n)) == NULL)
    goto fail;

  scaling = su_sf_bwlp(n, fc);

  for (i = 0; i < n + 1; ++i)
    b[i] *= scaling;

  if (!__su_iir_filt_init(filt, n + 1, a, n + 1, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (a != NULL)
    free(a);

  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_bwhpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc)
{
  SUFLOAT *a = NULL;
  SUFLOAT *b = NULL;
  SUFLOAT scaling;

  unsigned int i;

  if ((a = su_dcof_bwhp(n, fc)) == NULL)
    goto fail;

  if ((b = su_ccof_bwhp(n)) == NULL)
    goto fail;

  scaling = su_sf_bwhp(n, fc);

  for (i = 0; i < n + 1; ++i)
    b[i] *= scaling;

  if (!__su_iir_filt_init(filt, n + 1, a, n + 1, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (a != NULL)
    free(a);

  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_bwbpf_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT f1, SUFLOAT f2)
{
  SUFLOAT *a = NULL;
  SUFLOAT *b = NULL;
  SUFLOAT scaling;

  unsigned int i;

  if ((a = su_dcof_bwbp(n, f1, f2)) == NULL)
    goto fail;

  if ((b = su_ccof_bwbp(n)) == NULL)
    goto fail;

  scaling = su_sf_bwbp(n, f1, f2);

  for (i = 0; i < n + 1; ++i) {
    b[i] *= scaling;
  }

  if (!__su_iir_filt_init(filt, 2 * n + 1, a, 2 * n + 1, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (a != NULL)
    free(a);

  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_rrc_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT T, SUFLOAT beta)
{
  SUFLOAT *b = NULL;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof(SUFLOAT))) == NULL)
    goto fail;

  su_taps_rrc_init(b, T, beta, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_hilbert_init(su_iir_filt_t *filt, SUSCOUNT n)
{
  SUFLOAT *b = NULL;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof(SUFLOAT))) == NULL)
    goto fail;

  su_taps_hilbert_init(b, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_brickwall_bp_init(
    su_iir_filt_t *filt,
    SUSCOUNT n,
    SUFLOAT bw,
    SUFLOAT ifnor)
{
  SUFLOAT *b = NULL;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof(SUFLOAT))) == NULL)
    goto fail;

  su_taps_brickwall_bp_init(b, bw, ifnor, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}

SUBOOL
su_iir_brickwall_lp_init(su_iir_filt_t *filt, SUSCOUNT n, SUFLOAT fc)
{
  SUFLOAT *b = NULL;

  if (n < 1)
    goto fail;

  if ((b = malloc(n * sizeof(SUFLOAT))) == NULL)
    goto fail;

  su_taps_brickwall_lp_init(b, fc, n);

  if (!__su_iir_filt_init(filt, 0, NULL, n, b, SU_FALSE))
    goto fail;

  return SU_TRUE;

fail:
  if (b != NULL)
    free(b);

  return SU_FALSE;
}
