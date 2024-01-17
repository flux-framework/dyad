SUBDIRS = base64

noinst_LTLIBRARIES = libutils.la libmurmur3.la
libutils_la_SOURCES = utils.c utils.h read_all.c read_all.h
libutils_la_CFLAGS = \
    $(AM_CFLAGS) \
	$(FLUX_CORE_CFLAGS) \
    -fvisibility=hidden
libutils_la_LIBADD = \
    $(top_builddir)/src/utils/base64/libbase64.la \
    $(FLUX_CORE_LIBS)
libmurmur3_la_SOURCES = murmur3.c murmur3.h
libmurmur3_la_CFLAGS = \
    $(AM_CFLAGS) \
    -fvisibility=hidden
#libmurmur3_la_CPPFLAGS = $(AM_CPPFLAGS)
#libmurmur3_la_LIBADD =
