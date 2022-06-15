CONFIG = ../dyad.cfg
include $(CONFIG)

ifeq ($(DYAD_DEBUG),1)
  DYAD_OPTIONS += -DDYAD_FULL_DEBUG=1
  DYAD_OPTIONS += -DDYAD_LOGGING_ON=1
  $(info DYAD_DEBUG is enabled)
endif


_CFLAGS = $(CFLAGS) $(C_VER) $(DYAD_OPTIONS) -I./ -I../common -I$(FLUX_CORE_INCLUDES)
#_CXXFLAGS = $(CXXFLAGS) $(CXX_VER) $(DYAD_OPTIONS) -I./ -I../common -I$(FLUX_CORE_INCLUDES)
_LDFLAGS = $(LDFLAGS) -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined -shared $(FLUX_CORE_LIBS) -ldl
#--disable-static

ifeq ($(BUILD_TEST),1)
all: libdyad_sync.so flux_barrier
	+$(MAKE) -C test
else
all: libdyad_sync.so flux_barrier
endif

libdyad_sync.so: wrapper.o ../common/utils.o ../common/murmur3.o
	$(CC) $(_CFLAGS) $^ -o $@ $(_LDFLAGS)

wrapper.o: wrapper.c wrapper.h dyad_ctx.h ../common/utils.h ../common/murmur3.h
	$(CC) $(_CFLAGS) -DDYAD_CHECK=1 $< -c -o $@

cpa_dump: cpa_dump.c
	$(CC) $(_CFLAGS) $^ -o $@ -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined $(FLUX_CORE_LIBS)

flux_barrier: flux_barrier.c
	$(CC) $(_CFLAGS) $^ -o $@ -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined $(FLUX_CORE_LIBS)

.PHONY: clean install

install: libdyad_sync.so flux_barrier
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/bin
	install -m 640 libdyad_sync.so $(PREFIX)/lib
	install -m 750 flux_barrier $(PREFIX)/bin

ifeq ($(BUILD_TEST),1)
clean:
	@rm -f *.o *~ libdyad_sync.so cpa_dump flux_barrier
	+$(MAKE) clean -C test
else
clean:
	@rm -f *.o *~ libdyad_sync.so cpa_dump flux_barrier
endif
