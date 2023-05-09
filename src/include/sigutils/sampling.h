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

#ifndef _SIGUTILS_SAMPLING_H
#define _SIGUTILS_SAMPLING_H

#include <sigutils/types.h>

/*
 * Conventions used for frequency representation
 * ---------------------------------------------
 * SAMPLING FREQUENCY (fs): How many samples are taken per second, in Hz.
 *
 * NORMALIZED FREQUENCY (fnor): Represents the frequency of a discretized
 * signal, ranging from 0 (constant signal) to 1 (a signal that flips sign on
 * each time step). Units: hcps ("half cycles per sample")
 *
 * ABSOLUTE FREQUENCY (freq): Represents the actual frequency of the signal,
 * in Hz. It is defined as:
 *
 *   freq = fnor * fs / 2.
 *
 * NORMALIZED ANGULAR FREQUENCY (omrel): Represents how many radians are added
 * to the signal phase in each time step. It is defined as:
 *
 *   omrel = PI * fnor
 *
 * as the flipping sign signal's phase increments in 180 degrees on each
 * time step.
 */

#define SU_ABS2NORM_FREQ(fs, freq) (2 * (SUFLOAT)(freq) / (SUFLOAT)(fs))
#define SU_NORM2ABS_FREQ(fs, fnor) ((SUFLOAT)(fs) * (SUFLOAT)(fnor) / 2.)

/*
 * Normalized and absolute baud rates are transformed using a different
 * rule: since CDR block works with the symbol period, it's convenient
 * to define the baud rate as the inverse of the symbol period in samples.
 *
 * As a consequence of this, a normalized baudrate of 1 means 1 symbol per
 * sample.
 */

#define SU_ABS2NORM_BAUD(fs, freq) ((SUFLOAT)(freq) / (SUFLOAT)(fs))
#define SU_NORM2ABS_BAUD(fs, fnor) ((SUFLOAT)(fs) * (SUFLOAT)(fnor))

#define SU_NORM2ANG_FREQ(freq) (PI * (freq))
#define SU_ANG2NORM_FREQ(omrel) ((omrel) / (PI))

#define SU_T2N(fs, t) ((unsigned int)SU_FLOOR((t) * (SUFLOAT)(fs)))
#define SU_N2T(fs, n) ((unsigned int)SU_FLOOR((n) / (SUFLOAT)(fs)))

#define SU_T2N_COUNT(fs, t) ((unsigned int)SU_CEIL((t) * (SUFLOAT)(fs)))
#define SU_N2T_COUNT(fs, n) ((unsigned int)SU_CEIL((n) / (SUFLOAT)(fs)))

#define SU_T2N_FLOAT(fs, t) ((t) * (SUFLOAT)(fs))
#define SU_N2T_FLOAT(fs, n) ((n) / (SUFLOAT)(fs))

#endif /* _SIGUTILS_SAMPLING_H */
