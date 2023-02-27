lib_LTLIBRARIES = dyad.la
dyad_la_SOURCES = \
	dtl/dyad_mod_dtl.c \
	dtl/dyad_mod_dtl.h \
	dtl/flux_mod_dtl.c \
	dtl/flux_mod_dtl.h \
	dtl/ucx_mod_dtl.c \
	dtl/ucx_mod_dtl.h \
	dyad.c \
	dyad_flux_log.h
dyad_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module \
	-avoid-version \
	-no-undefined \
	-shared \
	-export-symbols-regex '^mod_(name|main)$$' \
    -Wl,-rpath,'$(UCX_LIBDIR)'
dyad_la_LIBADD = \
	$(top_builddir)/src/utils/libutils.la \
    $(UCX_LIBS) \
	$(JANSSON_LIBS) \
	$(FLUX_CORE_LIBS)
dyad_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/utils/base64 \
	$(UCX_CFLAGS) \
	$(JANSSON_CFLAGS) \
	$(FLUX_CORE_CFLAGS)
if PERFFLOW
dyad_la_LIBADD += $(PERFFLOW_LIBS)
dyad_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
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
urpc_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) -I$(top_builddir)/src/utils
if PERFFLOW
urpc_la_LIBADD += $(PERFFLOW_LIBS)
urpc_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif
endif

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) dyad.la)
