lib_LTLIBRARIES = libdyad_fstream.la

libdyad_fstream_la_SOURCES = dyad_stream_core.cpp
libdyad_fstream_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined
libdyad_fstream_la_LIBADD = $(top_builddir)/src/common/libutils.la \
                            $(top_builddir)/src/common/libmurmur3.la \
                            $(FLUX_CORE_LIBS)
libdyad_fstream_la_CPPFLAGS = $(AM_CPPFLAGS) \
                              -I$(top_builddir)/src/common \
                              $(FLUX_CORE_CFLAGS)
if PERFFLOW
libdyad_fstream_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_fstream_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

if URPC
lib_LTLIBRARIES += liburpc_client.la
liburpc_client_la_SOURCES = urpc_client.c urpc_client.h urpc_ctx.h
liburpc_client_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined
liburpc_client_la_LIBADD = $(top_builddir)/src/common/libutils.la \
                           $(top_builddir)/src/common/libb64_payload.la \
                           $(FLUX_CORE_LIBS) $(JANSSON_LIBS)
liburpc_client_la_CPPFLAGS = $(AM_CPPFLAGS) \
                             -I$(top_builddir)/src/common \
                             $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS)
if PERFFLOW
liburpc_client_la_LIBADD += $(PERFFLOW_LIBS)
liburpc_client_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif
endif

include_HEADERS = dyad_stream_core.hpp dyad_stream_api.hpp dyad_params.hpp
