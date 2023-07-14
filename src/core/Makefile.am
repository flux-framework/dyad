lib_LTLIBRARIES = libdyad_core.la
libdyad_core_la_SOURCES = \
	dyad_core.c
libdyad_core_la_LIBADD = \
	$(top_builddir)/src/dtl/libdyad_dtl.la \
	$(UCX_LIBS) \
	$(JANSSON_LIBS) \
	$(FLUX_CORE_LIBS)
libdyad_core_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
	-I$(top_srcdir)/src/dtl \
    $(UCX_CFLAGS) \
    $(JANSSON_CFLAGS) \
	$(FLUX_CORE_CFLAGS) \
	-DBUILDING_DYAD=1 \
	-fvisibility=hidden
libdyad_core_la_CPPFLAGS = 
libdyad_core_la_LDFLAGS = \
	-Wl,-rpath,'$(UCX_LIBDIR)' \
	$(AM_LDFLAGS)
if PERFFLOW
libdyad_core_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_core_la_CFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
libdyad_core_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif

include_HEADERS = dyad_core.h dyad_envs.h
