lib_LTLIBRARIES = libdyad_sync.la
libdyad_sync_la_SOURCES = wrapper.c wrapper.h
libdyad_sync_la_LIBADD = $(top_builddir)/src/core/libdyad_core.la
libdyad_sync_la_CPPFLAGS = -DDYAD_CHECK=1 -I$(top_builddir)/src/utils -I$(top_builddir)/src/core
if PERFFLOW
libdyad_sync_la_LIBADD += $(PERFFLOW_LIBS)
libdyad_sync_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
endif

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(FLUX_CORE_LIBS)
flux_barrier_CPPFLAGS = $(FLUX_CORE_CFLAGS)
