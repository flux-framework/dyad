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
    $(JANSSON_LIBS) \
    $(FLUX_CORE_LIBS)
libdyad_dtl_la_CFLAGS = \
    $(AM_CFLAGS) \
    -I$(top_srcdir)/src/utils \
    -I$(top_srcdir)/src/utils/base64 \
    $(JANSSON_CFLAGS) \
    $(FLUX_CORE_CLFAGS) \
    -fvisibility=hidden

if UCX
libdyad_dtl_la_SOURCES += \
    ucx_dtl.c \
    ucx_dtl.h
libdyad_dtl_la_LIBADD += $(UCX_LIBS)
libdyad_dtl_la_CFLAGS += $(UCX_CFLAGS) -DDYAD_ENABLE_UCX=1
endif

include_HEADERS = dyad_rc.h dyad_flux_log.h dyad_dtl.h