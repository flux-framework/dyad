lib_LTLIBRARIES = libdyad_sync.la
libdyad_sync_la_SOURCES = wrapper.c
libdyad_sync_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined -shared
libdyad_sync_la_LIBADD = $(top_builddir)/src/core/libdyad_core.la
libdyad_sync_la_CPPFLAGS = -I$(top_builddir)/src/utils -I$(top_builddir)/src/core $(FLUX_CORE_CFLAGS)

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) libdyad_sync.la)

# if PERFFLOW
# libdyad_sync_la_LIBADD += $(PERFFLOW_LIBS)
# libdyad_sync_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
# endif

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(AM_LDFLAGS) $(FLUX_CORE_LIBS)
flux_barrier_CPPFLAGS = $(AM_CPPFLAGS) $(FLUX_CORE_CFLAGS)
