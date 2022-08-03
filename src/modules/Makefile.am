lib_LTLIBRARIES = dyad.la
dyad_la_SOURCES = dyad.c read_all.c dyad_ctx.h read_all.h
dyad_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version -no-undefined -shared
dyad_la_LIBADD = $(top_builddir)/src/common/libutils.la $(FLUX_CORE_LIBS)
dyad_la_CPPFLAGS = $(AM_CPPFLAGS) \
                   -I$(top_builddir)/src/common \
                   $(FLUX_CORE_CFLAGS)
if PERFFLOW
dyad_la_LIBADD += $(PERFFLOW_LIBS)
dyad_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

if URPC
lib_LTLIBRARIES += urpc.la
urpc_la_SOURCES = read_all.c urpc.c urpc_ctx.h
urpc_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version -no-undefined -shared
urpc_la_LIBADD = $(top_builddir)/src/common/libutils.la \
                 $(top_builddir)/src/common/libb64_payload.la \
                 $(FLUX_CORE_LIBS) $(JANSSON_LIBS)
urpc_la_CPPFLAGS = $(AM_CPPFLAGS) \
                   -I$(top_builddir)/src/common \
                   $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS)
if PERFFLOW
urpc_la_LIBADD += $(PERFFLOW_LIBS)
urpc_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif
endif

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) $(lib_LTLIBRARIES))
