lib_LTLIBRARIES = libdyad_core.la
libdyad_core_la_SOURCES = \
	dtl/dyad_dtl.c \
	dtl/dyad_dtl.h \
	dtl/flux_dtl.c \
	dtl/flux_dtl.h \
	dtl/ucx_dtl.c \
	dtl/ucx_dtl.h \
	dyad_core.c
libdyad_core_la_LIBADD = \
    $(top_builddir)/src/utils/libutils.la \
	$(top_builddir)/src/utils/libmurmur3.la \
	$(UCX_LIBS) \
	$(JANSSON_LIBS) \
	$(FLUX_CORE_LIBS)
libdyad_core_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
    $(UCX_CFLAGS) \
    $(JANSSON_CFLAGS) \
	$(FLUX_CORE_CFLAGS)
libdyad_core_la_LDFLAGS = -export-symbols dyad_core.sym -Wl,-rpath,'$(UCX_LIBDIR)' $(AM_LDFLAGS)
if PERFFLOW
libdyad_core_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_core_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

include_HEADERS = dyad_core.h dyad_envs.h dyad_rc.h dyad_flux_log.h dyad_dtl_defs.h
