AUTOMAKE_OPTIONS = subdir-objects

noinst_LTLIBRARIES = libsigutils.la

libsigutils_la_CFLAGS = -I../util -I. -ggdb

libsigutils_la_SOURCES = agc.c agc.h blocks/agc.c blocks/pll.c blocks/tuner.c \
 blocks/wavfile.c block.c block.h coef.c coef.h iir.c iir.h lib.c ncqo.c \
 ncqo.h pll.c pll.h sampling.h sigutils.h taps.c taps.h types.h
