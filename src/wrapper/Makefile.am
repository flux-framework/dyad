lib_LTLIBRARIES = libdyad_sync.la
libdyad_sync_la_SOURCES = wrapper.c wrapper.h dyad_ctx.h
libdyad_sync_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version -no-undefined -shared
libdyad_sync_la_LIBADD = $(top_builddir)/src/common/libutils.la \
                         $(top_builddir)/src/common/libmurmur3.la \
                         $(FLUX_CORE_LIBS)
libdyad_sync_la_CPPFLAGS = $(AM_CPPFLAGS) \
                           -I$(top_builddir)/src/common \
                           $(FLUX_CORE_CFLAGS)

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) $(lib_LTLIBRARIES))

if PERFFLOW
libdyad_sync_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_sync_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(AM_LDFLAGS) $(FLUX_CORE_LIBS)
flux_barrier_CPPFLAGS = $(AM_CPPFLAGS) $(FLUX_CORE_CFLAGS)

noinst_PROGRAMS = cpa_dump
cpa_dump_SOURCES = cpa_dump.c
cpa_dump_LDADD = $(FLUX_CORE_LIBS)
cpa_dump_CPPFLAGS = $(FLUX_CORE_CFLAGS)
