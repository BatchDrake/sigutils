AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libsigutils.la

libsigutils_la_CFLAGS = -I../util -I. -ggdb @fftw3_CFLAGS@ @SNDFILE_CFLAGS@
libsigutils_la_LIBADD = ../util/libutil.la
libsigutils_la_SOURCES = agc.c agc.h blocks/agc.c blocks/clock.c blocks/pll.c  \
 blocks/tuner.c blocks/filt.c blocks/siggen.c blocks/wavfile.c block.c block.h \
 clock.c clock.h coef.c coef.h detect.c detect.h encoder.c encoder.h           \
 encoders/diff.c iir.c iir.h lfsr.c lfsr.h lib.c log.c log.h modem.c modem.h   \
 modems/qpsk.c ncqo.c ncqo.h pll.c pll.h property.c property.h sampling.h      \
 sigutils.h taps.c taps.h types.h equalizer.c equalizer.h
