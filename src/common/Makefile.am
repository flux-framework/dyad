SUBDIRS = libtap

noinst_LTLIBRARIES = libutils.la libmurmur3.la libb64_payload.la
libutils_la_SOURCES = utils.c utils.h dyad.h
libutils_la_CPPFLAGS = $(AM_CPPFLAGS) $(STRNCAT_SPECIFIC_CFLAGS)
#libutils_la_LIBADD =
libmurmur3_la_SOURCES = murmur3.c murmur3.h
#libmurmur3_la_CPPFLAGS = $(AM_CPPFLAGS)
#libmurmur3_la_LIBADD =

if URPC
libb64_payload_la_SOURCES = b64_payload.c b64_payload.h
libb64_payload_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(LIBB64_DIR)/include
libb64_payload_la_LIBADD = $(AM_LDFLAGS) -L$(LIBB64_DIR)/src -lb64
endif
