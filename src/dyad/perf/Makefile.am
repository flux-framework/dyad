noinst_LTLIBRARIES = libdyad_perf.la

libdyad_perf_la_SOURCES = dyad_perf.h
libdyad_perf_la_LIBADD = \
    $(FLUX_CORE_LIBS) \
    $(FLUX_OPTPARSE_LIBS)
libdyad_perf_la_CFLAGS = \
    $(AM_CFLAGS) \
    -I$(top_srcdir)/src/common \
    $(FLUX_CORE_CFLAGS) \
    $(FLUX_OPTPARSE_CFLAGS) \
    -fvisibility=hidden
libdyad_perf_la_CXXFLAGS = \
    $(AM_CFLAGS) \
    -I$(top_srcdir)/src/common \
    $(FLUX_CORE_CFLAGS) \
    $(FLUX_OPTPARSE_CFLAGS) \
    -fvisibility=hidden

if WITH_CALIPER
libdyad_perf_la_SOURCES += caliper_perf.cpp
libdyad_perf_la_LIBADD += \
    $(CALIPER_LIBS)
libdyad_perf_la_CFLAGS += \
    -DWITH_CALIPER=1 \
    $(CALIPER_CFLAGS)
libdyad_perf_la_CXXFLAGS += \
    -DWITH_CALIPER=1 \
    $(CALIPER_CFLAGS)
else
libdyad_perf_la_SOURCES += dyad_perf.c
endif