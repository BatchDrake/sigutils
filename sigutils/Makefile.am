AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libsigutils.la

libsigutils_la_CFLAGS = -I../util -I. -ggdb
libsigutils_la_LIBADD = ../util/libutil.la
libsigutils_la_SOURCES = agc.c agc.h blocks/agc.c blocks/clock.c blocks/pll.c \
 blocks/tuner.c blocks/filt.c blocks/wavfile.c block.c block.h clock.c clock.h \
 coef.c coef.h iir.c iir.h lib.c modem.c modem.h modems/qpsk.c ncqo.c ncqo.h \
 pll.c pll.h property.c property.h sampling.h sigutils.h taps.c taps.h types.h
