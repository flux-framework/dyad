lib_LTLIBRARIES = libdyad_fstream.la

libdyad_fstream_la_SOURCES = dyad_stream_core.cpp
libdyad_fstream_la_LDFLAGS = \
    $(AM_LDFLAGS) \
    -avoid-version \
    -no-undefined
libdyad_fstream_la_LIBADD = $(top_builddir)/src/core/libdyad_core.la
libdyad_fstream_la_CXXFLAGS = \
    $(AM_CFLAGS) \
    -I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/common \
    -I$(top_srcdir)/src/core \
	-I$(top_srcdir)/src/dtl \
    $(FLUX_CORE_CFLAGS)
libdyad_fstream_la_CPPFLAGS =
if PERFFLOW
libdyad_fstream_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_fstream_la_CXXFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
libdyad_fstream_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif

include_HEADERS = dyad_params.hpp dyad_stream_core.hpp dyad_stream_api.hpp
