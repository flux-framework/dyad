lib_LTLIBRARIES = dyad.la
dyad_la_SOURCES = dyad.c read_all.c dyad_ctx.h read_all.h
dyad_la_LDFLAGS = -module -avoid-version -no-undefined
dyad_la_LIBADD = $(top_builddir)/src/common/libutils.la $(FLUXCORE_LIBS)
dyad_la_CPPFLAGS = $(FLUXCORE_CFLAGS) -I$(top_builddir)/src/common
if HASPERFFLOW
dyad_la_LIBADD += $(PERFFLOW_LIBS)
dyad_la_CPPFLAGS += $(PERFFLOW_CFLAGS)
endif
