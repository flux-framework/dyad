lib_LTLIBRARIES = libdyad_sync.la
libdyad_sync_la_SOURCES = wrapper.c wrapper.h dyad_ctx.h
libdyad_sync_la_LIBADD = $(top_builddir)/src/common/libutils.la $(top_builddir)/src/common/libmurmur3.la $(FLUX_CORE_LIBS)
libdyad_sync_la_CPPFLAGS = $(FLUX_CORE_CFLAGS) -DDYAD_CHECK=1 -I$(top_builddir)/src/common

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(FLUX_CORE_LIBS)
flux_barrier_CPPFLAGS = $(FLUX_CORE_CFLAGS)

noinst_PROGRAMS = cpa_dump
cpa_dump_SOURCES = cpa_dump.c
cpa_dump_LDADD = $(FLUX_CORE_LIBS)
cpa_dump_CPPFLAGS = $(FLUX_CORE_CFLAGS)
