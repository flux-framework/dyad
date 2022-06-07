SUBDIRS = libtap

noinst_LTLIBRARIES = libutils.la libmurmur3.la
libutils_la_SOURCES = utils.c utils.h dyad.h
libutils_la_CPPFLAGS = $(FLUXCORE_CFLAGS)
libutils_la_LIBADD = $(FLUXCORE_LIBS)
libmurmur3_la_SOURCES = murmur3.c murmur3.h
libmurmur3_la_CPPFLAGS = $(FLUXCORE_CFLAGS)
libmurmur3_la_LIBADD = $(FLUXCORE_LIBS)
