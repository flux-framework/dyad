lib_LTLIBRARIES = libdyad_sync.la
libdyad_sync_la_SOURCES = wrapper.c wrapper.h dyad_ctx.h
libdyad_sync_la_LIBADD = $(top_builddir)/src/common/libutils.la $(top_builddir)/src/common/libmurmur3.la $(FLUXCORE_LIBS)
libdyad_sync_la_CPPFLAGS = $(FLUXCORE_CFLAGS) -DDYAD_CHECK=1

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(FLUXCORE_LIBS)
flux_barrier_CPPFLAGS = $(FLUXCORE_CFLAGS)

noinst_PROGRAMS = cpa_dump
cpa_dump_SOURCES = cpa_dump.c
cpa_dump_LDADD = $(FLUXCORE_LIBS)
cpa_dump_CPPFLAGS = $(FLUXCORE_CFLAGS)
