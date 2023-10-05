lib_LTLIBRARIES = dyad.la
dyad_la_SOURCES = \
	dyad.c
# Don't put a line break before DYAD_MOD_RPATH in case it evaluates to an empty string
dyad_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module \
	-avoid-version \
	-no-undefined \
	--disable-static \
	-shared \
	-export-dynamic $(DYAD_MOD_RPATH)
dyad_la_LIBADD = \
	$(top_builddir)/src/dtl/libdyad_dtl.la \
	$(JANSSON_LIBS) \
	$(FLUX_CORE_LIBS)
dyad_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
	-I$(top_srcdir)/src/dtl \
	$(JANSSON_CFLAGS) \
	$(FLUX_CORE_CFLAGS) \
	-DBUILDING_DYAD \
	-fvisibility=hidden
dyad_la_CPPFLAGS =
if UCX
dyad_la_LIBADD += $(UCX_LIBS)
dyad_la_CFLAGS += $(UCX_CFLAGS)
endif
if PERFFLOW
dyad_la_LIBADD += $(PERFFLOW_LIBS)
dyad_la_CFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
dyad_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif

if URPC
lib_LTLIBRARIES += urpc.la
urpc_la_SOURCES = urpc.c urpc_ctx.h
# TODO replace current LDFLAGS rule with this one once libb64 is required
# urpc_la_LDFLAGS = -module -avoid-version -no-undefined -L$(LIBB64_DIR)/src
urpc_la_LDFLAGS = -module -avoid-version -no-undefined
urpc_la_LIBADD = $(top_builddir)/src/utils/libutils.la $(FLUX_CORE_LIBS) $(JANSSON_LIBS)
# TODO uncomment once libb64 is required
# urpc_la_LIBS = -lb64
# TODO replace current CPPFLAGS rule with this one once libb64 is required
# urpc_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) -I$(LIBB64_DIR)/include -I$(top_builddir)/src/common
urpc_la_CFLAGS = $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) -I$(top_builddir)/src/utils
urpc_la_CPPFLAGS =
if PERFFLOW
urpc_la_LIBADD += $(PERFFLOW_LIBS)
urpc_la_CFLAGS += $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
urpc_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS)
endif
endif

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) dyad.la)
