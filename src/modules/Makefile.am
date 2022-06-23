lib_LTLIBRARIES = dyad.la
dyad_la_SOURCES = dyad.c read_all.c dyad_ctx.h read_all.h
dyad_la_LDFLAGS = -module -avoid-version -no-undefined
dyad_la_LIBADD = $(top_builddir)/src/common/libutils.la $(FLUX_CORE_LIBS)
dyad_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) -I$(top_builddir)/src/common
if PERFFLOW
dyad_la_LIBADD += $(PERFFLOW_LIBS)
dyad_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

if URPC
lib_LTLIBRARIES += urpc.la
urpc_la_SOURCES = read_all.c urpc.c urpc_ctx.h
# TODO replace current LDFLAGS rule with this one once libb64 is required
# urpc_la_LDFLAGS = -module -avoid-version -no-undefined -L$(LIBB64_DIR)/src
urpc_la_LDFLAGS = -module -avoid-version -no-undefined
urpc_la_LIBADD = $(top_builddir)/src/common/libutils.la $(FLUX_CORE_LIBS) $(JANSSON_LIBS)
# TODO uncomment once libb64 is required
# urpc_la_LIBS = -lb64
# TODO replace current CPPFLAGS rule with this one once libb64 is required
# urpc_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) -I$(LIBB64_DIR)/include -I$(top_builddir)/src/common
urpc_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) -I$(top_builddir)/src/common
if PERFFLOW
urpc_la_LIBADD += $(PERFFLOW_LIBS)
urpc_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif
endif
