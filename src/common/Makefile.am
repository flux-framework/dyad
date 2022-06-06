SUBDIRS = libtap

noinst_LTLIBRARIES = libutils.la libmurmur3.la
libutils_la_SOURCES = utils.c utils.h dyad.h
libutils_la_CPPFLAGS = $(FLUX_CORE_CFLAGS)
libutils_la_LIBADD = $(FLUX_CORE_LIBS)
libmurmur3_la_SOURCES = murmur3.c murmur3.h
libmurmur3_la_CPPFLAGS = $(FLUX_CORE_CFLAGS)
libmurmur3_la_LIBADD = $(FLUX_CORE_LIBS)
