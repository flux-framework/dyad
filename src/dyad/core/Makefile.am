lib_LTLIBRARIES = libdyad_core.la
libdyad_core_la_SOURCES = \
	dyad_core.c
libdyad_core_la_LIBADD = \
	$(top_builddir)/src/dtl/libdyad_dtl.la \
	$(JANSSON_LIBS) \
	$(FLUX_CORE_LIBS)
libdyad_core_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/dtl \
	-I$(top_srcdir)/src/perf \
	$(JANSSON_CFLAGS) \
	$(FLUX_CORE_CFLAGS) \
	-DBUILDING_DYAD=1 \
	-fvisibility=hidden
libdyad_core_la_CPPFLAGS =
if UCX
libdyad_core_la_LIBADD += $(UCX_LIBS)
libdyad_core_la_CFLAGS += $(UCX_CFLAGS)
endif
if PERFFLOW
libdyad_core_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_core_la_CFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
libdyad_core_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif
if WITH_CALIPER
libdyad_core_la_CFLAGS += \
    -DWITH_CALIPER=1 \
	$(CALIPER_CFLAGS)
libdyad_core_la_LIBADD += \
    $(CALIPER_LIBS)
endif

include_HEADERS = dyad_core.h dyad_envs.h
