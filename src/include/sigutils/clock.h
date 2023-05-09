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

#ifndef _SIGUTILS_CLOCK_H
#define _SIGUTILS_CLOCK_H

#include <sigutils/block.h>
#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sigutils_sampler {
  SUFLOAT bnor;
  SUFLOAT period;
  SUFLOAT phase;
  SUFLOAT phase0_rel;
  SUFLOAT phase0;
  SUCOMPLEX prev;
};

typedef struct sigutils_sampler su_sampler_t;

SUINLINE
SU_GETTER(su_sampler, SUFLOAT, get_period)
{
  return self->period;
}

SUINLINE
SU_METHOD(su_sampler, void, set_phase_addend, SUFLOAT addend)
{
  self->phase0_rel = SU_FLOOR(addend);
  self->phase = self->period * self->phase0_rel;
}

SUINLINE
SU_METHOD(su_sampler, SUBOOL, feed, SUCOMPLEX *sample)
{
  SUBOOL sampled = SU_FALSE;
  SUFLOAT alpha;
  SUCOMPLEX output = *sample, result;

  if (self->period >= 1.) {
    self->phase += 1.;
    if (self->phase >= self->period) {
      self->phase -= self->period;

      /* Interpolate with previous sample for improved accuracy */
      if (SU_FLOOR(self->phase) == 0) {
        alpha = self->phase - SU_FLOOR(self->phase);
        result = (1 - alpha) * self->prev + alpha * output;
        *sample = result;
        sampled = SU_TRUE;
      }
    }
  }

  self->prev = output;

  return sampled;
}

SU_CONSTRUCTOR(su_sampler, SUFLOAT bnor);
SU_DESTRUCTOR(su_sampler);

SU_METHOD(su_sampler, SUBOOL, set_rate, SUFLOAT bnor);
SU_METHOD(su_sampler, void, set_phase, SUFLOAT phase);

/*
 * The implementation of the Gardner clock recovery algorithm computes the
 * following clock error estimate:
 *
 * e = Re(x[n - 1/2] * conj(x[n] - x[n - 1]))
 *
 * If we take x[n] as vectors in the IQ diagram, this could be seen as a
 * scalar product between the sample in the transition from two symbols, and
 * the distance vector between those two symbols. If the clock is in sync,
 * there are essentially three cases here:
 *
 * 1. Two consecutive symbols are equal: distance is zero and error is zero
 *
 *       + <o>     T = (1, 1)
 *       | /       d = (0, 0)
 *       |/
 *    +--+--+
 *       |
 *       |
 *       +
 *
 * 2. There is a 90 deg shift between them: distance vector between them
 *    is perpendicular to the phase in the transition. Dot product is 0
 *
 *       +  o      T = (1, 0)
 *       |  |      d = (0, 2)
 *       |  |
 *    +--+--|>
 *       |  |
 *       |  v
 *       +  o
 *
 * 3. They have opposite sign: zero crossing, phase in the transition is 0
 *       +  o      T = (0, 0)
 *       | /       d = (2, 2)
 *       |/
 *    +--+--+
 *      /|
 *     / |
 *    o  +
 *
 * When this is not true, the perpendicularity between the transition and
 * distance vectors does not hold. The dot product returns a measure of
 * this disparity. The problem is, what do we do with this error signal?
 *
 * My first approach was to inject this error signal to both the
 * clock frequency and phase. As I don't want these two magnitudes
 * to oscillate around the transmitter parameters, I multiplied the error
 * signal by these two values:
 *
 * alpha = bandwidth (to be added to the phase)
 * beta  = .25 * bandwidth ^ 2 (to be added to the frequency)
 *
 * However, this doesn't seem to work. The system seems to behave highly
 * underdamped under this configuration.
 *
 * I found the following optimal values by trial-error, and I cannot give a
 * theoretical explanation (yet). Beta seems to depend linearly on alpha
 * for the critically damped case.
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (2e-1)
 * #define SU_CLOCK_BETA  (3e-5 * SU_CLOCK_ALPHA)
 *
 * Beta controls higher order derivative. Seems to be more like the
 * derivative of an exponential. If 0: no changes in baudrate.
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (2e-1)
 * #define SU_CLOCK_BETA  (3e-4 * SU_CLOCK_ALPHA)
 *
 * Falls to correct frequency even faster, slight oscillation around
 * desired baudrate (underdamping?). First zero crossing: 20.000.
 * Oscillation amplitude (approximate: +/-0.000005)
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (2e-1)
 * #define SU_CLOCK_BETA  (6e-4 * SU_CLOCK_ALPHA)
 *
 * Falls to correct frequency even faster, slight oscillation around
 * desired baudrate (underdamping?). Slightly bigger amplitude.
 * First zero crossing: 11267
 * Oscillation amplitude (approximate: +/-0.00001)
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (2e-1)
 * #define SU_CLOCK_BETA  (1.2e-3 * SU_CLOCK_ALPHA)
 *
 * Falls to correct frequency even faster, slight oscillation around
 * desired baudrate (underdamping?). Bigger oscillation.
 * First zero crossing: 8154
 * Oscillation amplitude (approximate: +/-0.00002)
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (1e-1)
 * #define SU_CLOCK_BETA  (3e-4 * SU_CLOCK_ALPHA)
 *
 * First zero crossing: 16195, actual stabilization in 27654
 * Oscillation amplitude (approximate: +/-0.0000025)
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (1e-1)
 * #define SU_CLOCK_BETA  (6e-4 * SU_CLOCK_ALPHA)
 *
 * First zero crossing: 10683
 * Oscillation amplitude (approximate: +/-0.000005)
 */

/*
 * OPTIMAL:
 * #define SU_CLOCK_ALPHA (1e-1)
 * #define SU_CLOCK_BETA  (1.2e-3 * SU_CLOCK_ALPHA)
 *
 * First zero crossing: 8074
 * Oscillation amplitude (approximate: +/-0.00001)
 */

#define SU_PREFERED_CLOCK_ALPHA (2e-1)
#define SU_PREFERED_CLOCK_BETA (6e-4 * SU_PREFERED_CLOCK_ALPHA)

enum sigutils_clock_detector_algorithm {
  SU_CLOCK_DETECTOR_ALGORITHM_NONE,
  SU_CLOCK_DETECTOR_ALGORITHM_GARDNER
};

struct sigutils_clock_detector {
  enum sigutils_clock_detector_algorithm algo;
  SUFLOAT alpha;           /* Damping factor for phase */
  SUFLOAT beta;            /* Damping factor for frequency */
  SUFLOAT bnor;            /* Normalized baud rate */
  SUFLOAT bmin;            /* Minimum baud rate */
  SUFLOAT bmax;            /* Maximum baud rate */
  SUFLOAT phi;             /* Symbol phase [0, 1/2)  */
  SUFLOAT gain;            /* Loop gain */
  SUFLOAT e;               /* Current error signal (debugging) */
  su_stream_t sym_stream;  /* Resampled signal */
  su_off_t sym_stream_pos; /* Read position in the symbol stream */
  SUBOOL halfcycle;        /* True if setting halfcycle */

  SUCOMPLEX x[3]; /* Previous symbol */
  SUCOMPLEX prev; /* Previous sample, for interpolation */
};

typedef struct sigutils_clock_detector su_clock_detector_t;

#define su_clock_detector_INITIALIZER                      \
  {                                                        \
    SU_CLOCK_DETECTOR_ALGORITHM_NONE, /* algo */           \
        SU_PREFERED_CLOCK_ALPHA,      /* alpha */          \
        SU_PREFERED_CLOCK_BETA,       /* beta */           \
        0.0,                          /* bnor */           \
        0.0,                          /* bmin */           \
        1.0,                          /* bmax */           \
        0.0,                          /* phi */            \
        1.0,                          /* loop gain */      \
        0.0,                          /* error signal */   \
        su_stream_INITIALIZER,        /* sym_stream */     \
        0,                            /* sym_stream_pos */ \
        SU_FALSE,                     /* halfcycle */      \
        {0, 0, 0},                    /* x */              \
        0,                            /* prev */           \
  }

SU_CONSTRUCTOR(
    su_clock_detector,
    SUFLOAT loop_gain,
    SUFLOAT bhint,
    SUSCOUNT bufsiz);
SU_DESTRUCTOR(su_clock_detector);

SU_METHOD(su_clock_detector, void, set_baud, SUFLOAT bnor);
SU_METHOD(su_clock_detector, void, feed, SUCOMPLEX val);
SU_METHOD(su_clock_detector, SUBOOL, set_bnor_limits, SUFLOAT lo, SUFLOAT hi);
SU_METHOD(su_clock_detector, SUSDIFF, read, SUCOMPLEX *buf, size_t size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_CLOCK_H */
