noinst_LTLIBRARIES = libdyad_dtl.la
libdyad_dtl_la_SOURCES = \
    dyad_dtl_impl.c \
    dyad_dtl_impl.h \
    dyad_rc.h \
    flux_dtl.c \
    flux_dtl.h
libdyad_dtl_la_LIBADD = \
    $(top_builddir)/src/utils/libutils.la \
    $(top_builddir)/src/utils/libmurmur3.la \
    $(top_builddir)/src/perf/libdyad_perf.la \
    $(JANSSON_LIBS) \
    $(FLUX_CORE_LIBS)
libdyad_dtl_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/perf \
	$(JANSSON_CFLAGS) \
	$(FLUX_CORE_CLFAGS) \
	-fvisibility=hidden
if WITH_CALIPER
libdyad_dtl_la_CFLAGS += \
    -DWITH_CALIPER=1 \
	$(CALIPER_CFLAGS)
libdyad_dtl_la_LIBADD += \
    $(CALIPER_LIBS)
endif

if UCX
libdyad_dtl_la_SOURCES += \
    ucx_dtl.c \
    ucx_dtl.h
libdyad_dtl_la_LIBADD += $(UCX_LIBS)
libdyad_dtl_la_CFLAGS += $(UCX_CFLAGS) -DDYAD_ENABLE_UCX=1
endif

include_HEADERS = dyad_dtl.h
