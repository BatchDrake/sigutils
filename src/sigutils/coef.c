/*
 *              COPYRIGHT
 *
 *  liir - Recursive digital filter functions
 *  Copyright (C) 2007 Exstrom Laboratories LLC
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  A copy of the GNU General Public License is available on the internet at:
 *
 *  http://www.gnu.org/copyleft/gpl.html
 *
 *  or you can write to:
 *
 *  The Free Software Foundation, Inc.
 *  675 Mass Ave
 *  Cambridge, MA 02139, USA
 *
 *  You can contact Exstrom Laboratories LLC via Email at:
 *
 *  stefan(AT)exstrom.com
 *
 *  or you can write to:
 *
 *  Exstrom Laboratories LLC
 *  P.O. Box 7651
 *  Longmont, CO 80501, USA
 *
 */

#define SU_LOG_DOMAIN "coef"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sigutils/iir.h>
#include <sigutils/log.h>

/**********************************************************************
  binomial_mult - multiplies a series of binomials together and returns
  the coefficients of the resulting polynomial.

  The multiplication has the following form:

  (x+p[0])*(x+p[1])*...*(x+p[n-1])

  The p[i] coefficients are assumed to be complex and are passed to the
  function as a pointer to an array of SUFLOATs of length 2n.

  The resulting polynomial has the following form:

  x^n + a[0]*x^n-1 + a[1]*x^n-2 + ... +a[n-2]*x + a[n-1]

  The a[i] coefficients can in general be complex but should in most
  cases turn out to be real. The a[i] coefficients are returned by the
  function as a pointer to an array of SUFLOATs of length 2n. Storage
  for the array is allocated by the function and should be freed by the
  calling program when no longer needed.

  Function arguments:

  n  -  The number of binomials to multiply
  p  -  Pointer to an array of SUFLOATs where p[2i] (i=0...n-1) is
    assumed to be the real part of the coefficient of the ith binomial
    and p[2i + 1] is assumed to be the imaginary part. The overall size
    of the array is then 2n.
*/

SUPRIVATE SUFLOAT *
su_binomial_mult(int n, const SUFLOAT *p)
{
  int i, j;
  SUFLOAT *a;

  if ((a = calloc(2 * n, sizeof(SUFLOAT))) == NULL)
    return NULL;

  for (i = 0; i < n; ++i) {
    for (j = i; j > 0; --j) {
      a[2 * j] += p[2 * i] * a[2 * (j - 1)] - p[2 * i + 1] * a[2 * (j - 1) + 1];
      a[2 * j + 1] +=
          p[2 * i] * a[2 * (j - 1) + 1] + p[2 * i + 1] * a[2 * (j - 1)];
    }

    a[0] += p[2 * i];
    a[1] += p[2 * i + 1];
  }

  return a;
}

/**********************************************************************
  trinomial_mult - multiplies a series of trinomials together and returns
  the coefficients of the resulting polynomial.

  The multiplication has the following form:

  (x^2 + b[0]x + c[0])*(x^2 + b[1]x + c[1])*...*(x^2 + b[n-1]x + c[n-1])

  The b[i] and c[i] coefficients are assumed to be complex and are passed
  to the function as a pointers to arrays of SUFLOATs of length 2n. The real
  part of the coefficients are stored in the even numbered elements of the
  array and the imaginary parts are stored in the odd numbered elements.

  The resulting polynomial has the following form:

  x^2n + a[0]*x^2n-1 + a[1]*x^2n-2 + ... +a[2n-2]*x + a[2n-1]

  The a[i] coefficients can in general be complex but should in most cases
  turn out to be real. The a[i] coefficients are returned by the function as
  a pointer to an array of SUFLOATs of length 4n. The real and imaginary
  parts are stored, respectively, in the even and odd elements of the array.
  Storage for the array is allocated by the function and should be freed by
  the calling program when no longer needed.

  Function arguments:

  n  -  The number of trinomials to multiply
  b  -  Pointer to an array of SUFLOATs of length 2n.
  c  -  Pointer to an array of SUFLOATs of length 2n.
*/

SUPRIVATE SUFLOAT *
su_trinomial_mult(int n, const SUFLOAT *b, const SUFLOAT *c)
{
  int i, j;
  SUFLOAT *a;

  if ((a = malloc(4 * n * sizeof(SUFLOAT))) == NULL)
    return NULL;

  a[2] = c[0];
  a[3] = c[1];
  a[0] = b[0];
  a[1] = b[1];

  for (i = 1; i < n; ++i) {
    a[2 * (2 * i + 1)] +=
        c[2 * i] * a[2 * (2 * i - 1)] - c[2 * i + 1] * a[2 * (2 * i - 1) + 1];

    a[2 * (2 * i + 1) + 1] +=
        c[2 * i] * a[2 * (2 * i - 1) + 1] + c[2 * i + 1] * a[2 * (2 * i - 1)];

    for (j = 2 * i; j > 1; --j) {
      a[2 * j] += b[2 * i] * a[2 * (j - 1)] - b[2 * i + 1] * a[2 * (j - 1) + 1]
                  + c[2 * i] * a[2 * (j - 2)]
                  - c[2 * i + 1] * a[2 * (j - 2) + 1];

      a[2 * j + 1] +=
          b[2 * i] * a[2 * (j - 1) + 1] + b[2 * i + 1] * a[2 * (j - 1)]
          + c[2 * i] * a[2 * (j - 2) + 1] + c[2 * i + 1] * a[2 * (j - 2)];
    }

    a[2] += b[2 * i] * a[0] - b[2 * i + 1] * a[1] + c[2 * i];
    a[3] += b[2 * i] * a[1] + b[2 * i + 1] * a[0] + c[2 * i + 1];
    a[0] += b[2 * i];
    a[1] += b[2 * i + 1];
  }

  return a;
}

/**********************************************************************
  dcof_bwlp - calculates the d coefficients for a butterworth lowpass
  filter. The coefficients are returned as an array of SUFLOATs.

*/

SUFLOAT *
su_dcof_bwlp(int n, SUFLOAT fcf)
{
  int k;
  SUFLOAT theta;
  SUFLOAT st;
  SUFLOAT ct;
  SUFLOAT parg;
  SUFLOAT sparg;
  SUFLOAT cparg;
  SUFLOAT a;
  SUFLOAT *rcof = NULL;
  SUFLOAT *dcof = NULL;

  SU_ALLOCATE_MANY(rcof, 2 * n, SUFLOAT);

  theta = M_PI * fcf;
  st = SU_SIN(theta);
  ct = SU_COS(theta);

  for (k = 0; k < n; ++k) {
    parg = M_PI * (SUFLOAT)(2 * k + 1) / (SUFLOAT)(2 * n);
    sparg = SU_SIN(parg);
    cparg = SU_COS(parg);
    a = 1.0 + st * sparg;
    rcof[2 * k] = -ct / a;
    rcof[2 * k + 1] = -st * cparg / a;
  }

  if ((dcof = su_binomial_mult(n, rcof)) == NULL)
    goto done;

  dcof[1] = dcof[0];
  dcof[0] = 1.0;

  for (k = 3; k <= n; ++k)
    dcof[k] = dcof[2 * k - 2];

done:
  if (rcof != NULL)
    free(rcof);

  return dcof;
}

/**********************************************************************
  dcof_bwhp - calculates the d coefficients for a butterworth highpass
  filter. The coefficients are returned as an array of SUFLOATs.

*/

SUFLOAT *
su_dcof_bwhp(int n, SUFLOAT fcf)
{
  return su_dcof_bwlp(n, fcf);
}

/**********************************************************************
  dcof_bwbp - calculates the d coefficients for a butterworth bandpass
  filter. The coefficients are returned as an array of SUFLOATs.

*/

SUFLOAT *
su_dcof_bwbp(int n, SUFLOAT f1f, SUFLOAT f2f)
{
  int k;
  SUFLOAT theta;
  SUFLOAT cp;
  SUFLOAT st;
  SUFLOAT ct;
  SUFLOAT s2t;
  SUFLOAT c2t;
  SUFLOAT *rcof = NULL;
  SUFLOAT *tcof = NULL;
  SUFLOAT *dcof = NULL;
  SUFLOAT parg;
  SUFLOAT sparg;
  SUFLOAT cparg;
  SUFLOAT a;

  cp = SU_COS(M_PI * (f2f + f1f) / 2.0);
  theta = M_PI * (f2f - f1f) / 2.0;
  st = SU_SIN(theta);
  ct = SU_COS(theta);
  s2t = 2.0 * st * ct;
  c2t = 2.0 * ct * ct - 1.0;

  SU_ALLOCATE_MANY(rcof, 2 * n, SUFLOAT);
  SU_ALLOCATE_MANY(tcof, 2 * n, SUFLOAT);

  for (k = 0; k < n; ++k) {
    parg = M_PI * (SUFLOAT)(2 * k + 1) / (SUFLOAT)(2 * n);
    sparg = SU_SIN(parg);
    cparg = SU_COS(parg);
    a = 1.0 + s2t * sparg;
    rcof[2 * k] = c2t / a;
    rcof[2 * k + 1] = s2t * cparg / a;
    tcof[2 * k] = -2.0 * cp * (ct + st * sparg) / a;
    tcof[2 * k + 1] = -2.0 * cp * st * cparg / a;
  }

  if ((dcof = su_trinomial_mult(n, tcof, rcof)) == NULL)
    goto done;

  dcof[1] = dcof[0];
  dcof[0] = 1.0;

  for (k = 3; k <= 2 * n; ++k)
    dcof[k] = dcof[2 * k - 2];

done:
  if (rcof != NULL)
    free(rcof);

  if (tcof != NULL)
    free(tcof);

  return dcof;
}

/**********************************************************************
  dcof_bwbs - calculates the d coefficients for a butterworth bandstop
  filter. The coefficients are returned as an array of SUFLOATs.

*/

SUFLOAT *
su_dcof_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f)
{
  int k;
  SUFLOAT theta;
  SUFLOAT cp;
  SUFLOAT st;
  SUFLOAT ct;
  SUFLOAT s2t;
  SUFLOAT c2t;
  SUFLOAT *rcof = NULL;
  SUFLOAT *tcof = NULL;
  SUFLOAT *dcof = NULL;
  SUFLOAT parg;
  SUFLOAT sparg;
  SUFLOAT cparg;
  SUFLOAT a;

  cp = SU_COS(M_PI * (f2f + f1f) / 2.0);
  theta = M_PI * (f2f - f1f) / 2.0;
  st = SU_SIN(theta);
  ct = SU_COS(theta);
  s2t = 2.0 * st * ct;
  c2t = 2.0 * ct * ct - 1.0;

  SU_ALLOCATE_MANY(rcof, 2 * n, SUFLOAT);
  SU_ALLOCATE_MANY(tcof, 2 * n, SUFLOAT);

  for (k = 0; k < n; ++k) {
    parg = M_PI * (SUFLOAT)(2 * k + 1) / (SUFLOAT)(2 * n);
    sparg = SU_SIN(parg);
    cparg = SU_COS(parg);
    a = 1.0 + s2t * sparg;
    rcof[2 * k] = c2t / a;
    rcof[2 * k + 1] = -s2t * cparg / a;
    tcof[2 * k] = -2.0 * cp * (ct + st * sparg) / a;
    tcof[2 * k + 1] = 2.0 * cp * st * cparg / a;
  }

  if ((dcof = su_trinomial_mult(n, tcof, rcof)) == NULL)
    goto done;

  dcof[1] = dcof[0];
  dcof[0] = 1.0;
  for (k = 3; k <= 2 * n; ++k)
    dcof[k] = dcof[2 * k - 2];

done:
  if (tcof != NULL)
    free(tcof);

  if (rcof != NULL)
    free(rcof);

  return dcof;
}

/**********************************************************************
  ccof_bwlp - calculates the c coefficients for a butterworth lowpass
  filter.

*/

SUFLOAT *
su_ccof_bwlp(int n)
{
  SUFLOAT *ccof;
  int m;
  int i;

  SU_ALLOCATE_MANY_CATCH(ccof, n + 1, SUFLOAT, return NULL);

  ccof[0] = 1;
  ccof[1] = n;
  m = n / 2;

  for (i = 2; i <= m; ++i) {
    ccof[i] = (n - i + 1) * (int)ccof[i - 1] / i;
    ccof[n - i] = ccof[i];
  }

  ccof[n - 1] = n;
  ccof[n] = 1;

  return ccof;
}

/**********************************************************************
  ccof_bwhp - calculates the c coefficients for a butterworth highpass
  filter.

*/

SUFLOAT *
su_ccof_bwhp(int n)
{
  SUFLOAT *ccof;
  int i;

  if ((ccof = su_ccof_bwlp(n)) == NULL)
    return NULL;

  for (i = 0; i <= n; ++i)
    if (i & 1)
      ccof[i] = -ccof[i];

  return ccof;
}

/**********************************************************************
  ccof_bwbp - calculates the c coefficients for a butterworth bandpass
  filter.

*/

SUFLOAT *
su_ccof_bwbp(int n)
{
  SUFLOAT *tcof = NULL;
  SUFLOAT *ccof = NULL;
  int i;

  if ((tcof = su_ccof_bwhp(n)) == NULL)
    goto done;

  SU_ALLOCATE_MANY(ccof, 2 * n + 1, SUFLOAT);

  for (i = 0; i < n; ++i) {
    ccof[2 * i] = tcof[i];
    ccof[2 * i + 1] = 0.0;
  }

  ccof[2 * n] = tcof[n];

done:
  if (tcof != NULL)
    free(tcof);

  return ccof;
}

/**********************************************************************
  ccof_bwbs - calculates the c coefficients for a butterworth bandstop
  filter.

*/

SUFLOAT *
su_ccof_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f)
{
  SUFLOAT alpha;
  SUFLOAT *ccof;
  int i, j;

  alpha = -2.0 * SU_COS(M_PI * (f2f + f1f) / 2.0)
          / SU_COS(M_PI * (f2f - f1f) / 2.0);

  SU_ALLOCATE_MANY_CATCH(ccof, 2 * n + 1, SUFLOAT, return NULL);

  ccof[0] = 1.0;

  ccof[2] = 1.0;
  ccof[1] = alpha;

  for (i = 1; i < n; ++i) {
    ccof[2 * i + 2] += ccof[2 * i];
    for (j = 2 * i; j > 1; --j)
      ccof[j + 1] += alpha * ccof[j] + ccof[j - 1];

    ccof[2] += alpha * ccof[1] + 1.0;
    ccof[1] += alpha;
  }

  return ccof;
}

/**********************************************************************
  sf_bwlp - calculates the scaling factor for a butterworth lowpass filter.
  The scaling factor is what the c coefficients must be multiplied by so
  that the filter response has a maximum value of 1.

*/

SUFLOAT
su_sf_bwlp(int n, SUFLOAT fcf)
{
  int k;
  SUFLOAT omega;
  SUFLOAT fomega;
  SUFLOAT parg0;
  SUFLOAT sf;

  omega = M_PI * fcf;
  fomega = SU_SIN(omega);
  parg0 = M_PI / (SUFLOAT)(2 * n);

  sf = 1.0;
  for (k = 0; k < n / 2; ++k)
    sf *= 1.0 + fomega * SU_SIN((SUFLOAT)(2 * k + 1) * parg0);

  fomega = SU_SIN(omega / 2.0);

  if (n & 1)
    sf *= fomega + SU_COS(omega / 2.0);

  sf = SU_POW(fomega, n) / sf;

  return sf;
}

/**********************************************************************
  sf_bwhp - calculates the scaling factor for a butterworth highpass filter.
  The scaling factor is what the c coefficients must be multiplied by so
  that the filter response has a maximum value of 1.

*/

SUFLOAT
su_sf_bwhp(int n, SUFLOAT fcf)
{
  int k;
  SUFLOAT omega;
  SUFLOAT fomega;
  SUFLOAT parg0;
  SUFLOAT sf;

  omega = M_PI * fcf;
  fomega = SU_SIN(omega);
  parg0 = M_PI / (SUFLOAT)(2 * n);

  sf = 1.0;

  for (k = 0; k < n / 2; ++k)
    sf *= 1.0 + fomega * SU_SIN((SUFLOAT)(2 * k + 1) * parg0);

  fomega = SU_COS(omega / 2.0);

  if (n & 1)
    sf *= fomega + SU_SIN(omega / 2.0);

  sf = SU_POW(fomega, n) / sf;

  return sf;
}

/**********************************************************************
  sf_bwbp - calculates the scaling factor for a butterworth bandpass filter.
  The scaling factor is what the c coefficients must be multiplied by so
  that the filter response has a maximum value of 1.

*/

SUFLOAT
su_sf_bwbp(int n, SUFLOAT f1f, SUFLOAT f2f)
{
  int k;
  SUFLOAT ctt;
  SUFLOAT sfr, sfi;
  SUFLOAT parg;
  SUFLOAT sparg;
  SUFLOAT cparg;
  SUFLOAT a, b, c;

  ctt = 1.0 / SU_TAN(M_PI * (f2f - f1f) / 2.0);
  sfr = 1.0;
  sfi = 0.0;

  for (k = 0; k < n; ++k) {
    parg = M_PI * (SUFLOAT)(2 * k + 1) / (SUFLOAT)(2 * n);
    sparg = ctt + SU_SIN(parg);
    cparg = SU_COS(parg);
    a = (sfr + sfi) * (sparg - cparg);
    b = sfr * sparg;
    c = -sfi * cparg;
    sfr = b - c;
    sfi = a - b - c;
  }

  return 1.0 / sfr;
}

/**********************************************************************
  sf_bwbs - calculates the scaling factor for a butterworth bandstop filter.
  The scaling factor is what the c coefficients must be multiplied by so
  that the filter response has a maximum value of 1.

*/

SUFLOAT
su_sf_bwbs(int n, SUFLOAT f1f, SUFLOAT f2f)
{
  int k;
  SUFLOAT tt;
  SUFLOAT sfr, sfi;
  SUFLOAT parg;
  SUFLOAT sparg;
  SUFLOAT cparg;
  SUFLOAT a, b, c;

  tt = SU_TAN(M_PI * (f2f - f1f) / 2.0);
  sfr = 1.0;
  sfi = 0.0;

  for (k = 0; k < n; ++k) {
    parg = M_PI * (SUFLOAT)(2 * k + 1) / (SUFLOAT)(2 * n);
    sparg = tt + SU_SIN(parg);
    cparg = SU_COS(parg);
    a = (sfr + sfi) * (sparg - cparg);
    b = sfr * sparg;
    c = -sfi * cparg;
    sfr = b - c;
    sfi = a - b - c;
  }

  return 1.0 / sfr;
}
