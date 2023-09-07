lib_LTLIBRARIES = liburpc_client.la
liburpc_client_la_SOURCES = urpc_client.c urpc_client.hpp
liburpc_client_la_LIBADD = $(FLUX_CORE_LIBS) $(top_builddir)/src/utils/libutils.la
liburpc_client_la_CFLAGS = $(FLUX_CORE_CFLAGS) -DURPC_CHECK=1 -I$(top_builddir)/src/utils
liburpc_client_la_CPPFLAGS =
if PERFFLOW
liburpc_client_la_LIBADD += $(PERFFLOW_LIBS)
liburpc_client_la_CFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
liburpc_client_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif
