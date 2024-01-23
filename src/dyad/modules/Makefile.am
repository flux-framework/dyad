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
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/dtl \
	-I$(top_srcdir)/src/perf \
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
if WITH_CALIPER
dyad_la_CFLAGS += \
	-DWITH_CALIPER=1 \
	$(CALIPER_CFLAGS)
dyad_la_LIBADD += $(CALIPER_LIBS)
endif

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) dyad.la)
