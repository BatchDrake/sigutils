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

#ifndef _SIGUTILS_NCQO_H
#define _SIGUTILS_NCQO_H

#include "types.h"

/* The numerically-controlled quadruature oscillator definition */
struct sigutils_ncqo {
  SUSCOUNT n; /* Sample count */
  SUFLOAT phi;
  SUFLOAT omega; /* Relative angular frequency */

  SUBOOL  sin_updated;
  SUFLOAT sin;

  SUBOOL  cos_updated;
  SUFLOAT cos;
};

typedef struct sigutils_ncqo su_ncqo_t;

#define su_ncqo_INITIALIZER {0, 0, 0, 0, 0}

/* Methods */

/* NCQO constructor */
void su_ncqo_init(su_ncqo_t *ncqo, SUFLOAT frel);

/* Compute next step */
void su_ncqo_step(su_ncqo_t *ncqo);

/* Force phase */
void su_ncqo_set_phase(su_ncqo_t *ncqo, SUFLOAT phi);

/* Get in-phase component */
SUFLOAT su_ncqo_get_i(su_ncqo_t *ncqo);

/* Get cuadrature component */
SUFLOAT su_ncqo_get_q(su_ncqo_t *ncqo);

/* Get both components as complex */
SUCOMPLEX su_ncqo_get(su_ncqo_t *ncqo);

/* Read (compute next + get) in-phase component */
SUFLOAT su_ncqo_read_i(su_ncqo_t *ncqo);

/* Read (compute next + get) cuadrature component */
SUFLOAT su_ncqo_read_q(su_ncqo_t *ncqo);

/* Read (compute next + get) both components as complex */
SUCOMPLEX su_ncqo_read(su_ncqo_t *ncqo);

/* Set oscillator frequency (relative angular freq) */
void su_ncqo_set_angfreq(su_ncqo_t *ncqo, SUFLOAT omrel);

/* Increase or decrease current frequency (relative angular freq) */
void su_ncqo_inc_angfreq(su_ncqo_t *ncqo, SUFLOAT delta);

/* Get current frequency (relative angular freq) */
SUFLOAT su_ncqo_get_angfreq(const su_ncqo_t *ncqo);

/* Set oscillator frequency (relative freq) */
void su_ncqo_set_freq(su_ncqo_t *ncqo, SUFLOAT frel);

/* Increase or decrease current frequency (relative freq) */
void su_ncqo_inc_freq(su_ncqo_t *ncqo, SUFLOAT delta);

/* Get current frequency (relative freq) */
SUFLOAT su_ncqo_get_freq(const su_ncqo_t *ncqo);

#endif /* _SIGUTILS_NCQO_H */
