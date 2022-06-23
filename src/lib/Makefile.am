lib_LTLIBRARIES = libdyad_fstream.la

libdyad_fstream_la_SOURCES = dyad_stream_core.cpp
libdyad_fstream_la_LIBADD = $(FLUX_CORE_LIBS) $(top_builddir)/src/common/libutils.la $(top_builddir)/src/common/libmurmur3.la
libdyad_fstream_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) -DDYAD_CHECK=1 -I$(top_builddir)/src/common
if PERFFLOW
libdyad_fstream_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_fstream_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

if URPC
lib_LTLIBRARIES += liburpc_client.la
liburpc_client_la_SOURCES = urpc_client.c urpc_client.hpp
liburpc_client_la_LIBADD = $(FLUX_CORE_LIBS) $(top_builddir)/src/common/libutils.la
liburpc_client_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) -DURPC_CHECK=1 -I$(top_builddir)/src/common
if PERFFLOW
liburpc_client_la_LIBADD += $(PERFFLOW_LIBS)
liburpc_client_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif
endif

include_HEADERS = dyad_stream_core.hpp dyad_stream_api.hpp dyad_params.hpp
