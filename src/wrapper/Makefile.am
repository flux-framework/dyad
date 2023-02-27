lib_LTLIBRARIES = dyad_wrapper.la
dyad_wrapper_la_SOURCES = wrapper.c
dyad_wrapper_la_LDFLAGS = \
	-Wl,-rpath,'$(UCX_LIBDIR)' \
	$(AM_LDFLAGS) \
	-module \
	-avoid-version \
	-no-undefined \
	-shared \
	-export-symbols wrapper.sym
dyad_wrapper_la_LIBADD = $(top_builddir)/src/core/libdyad_core.la
dyad_wrapper_la_CPPFLAGS = -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/core $(FLUX_CORE_CFLAGS)

install-exec-hook:
	@(cd $(DESTDIR)$(libdir) && $(RM) dyad_wrapper.la)

# if PERFFLOW
# dyad_wrapper_la_LIBADD += $(PERFFLOW_LIBS)
# dyad_wrapper_la_CPPFLAGS += $(PERFFLOW_PLUGIN_CPPFLAGS) $(PERFFLOW_CFLAGS) -DDYAD_PERFFLOW=1
# endif

bin_PROGRAMS = flux_barrier
flux_barrier_SOURCES = flux_barrier.c
flux_barrier_LDADD = $(AM_LDFLAGS) $(FLUX_CORE_LIBS)
flux_barrier_CPPFLAGS = $(AM_CPPFLAGS) $(FLUX_CORE_CFLAGS)
