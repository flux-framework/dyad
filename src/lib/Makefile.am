lib_LTLIBRARIES = libdyad_fstream.la
libdyad_fstream_la_SOURCES = dyad_stream_core.cpp
libdyad_fstream_la_LIBADD = $(FLUXCORE_LIBS) $(top_builddir)/src/common/libutils.la $(top_builddir)/src/common/libmurmur3.la
libdyad_fstream_la_CPPFLAGS = $(FLUXCORE_CFLAGS) -DDYAD_CHECK=1 -I$(top_builddir)/src/common
if HASPERFFLOW
libdyad_fstream_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_fstream_la_CPPFLAGS += $(PERFFLOW_CFLAGS)
endif
include_HEADERS = dyad_stream_core.hpp dyad_stream_api.hpp dyad_params.hpp
