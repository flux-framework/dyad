CONFIG = ../dyad.cfg
include $(CONFIG)

ifeq ($(DYAD_DEBUG),1)
  DYAD_OPTIONS += -DDYAD_FULL_DEBUG=1
  DYAD_OPTIONS += -DDYAD_LOGGING_ON=1
  $(info DYAD_DEBUG is enabled)
endif

_CFLAGS = $(CFLAGS) $(C_VER) $(DYAD_OPTIONS) -I../common -I$(FLUX_CORE_INCLUDES)
_LDFLAGS += $(LDFLAGS) -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined -shared $(FLUX_CORE_LIBS) -ldl
# --disable-static

all: dyad.so
	+$(MAKE) -C test

dyad.so: dyad.o ../common/utils.o read_all.o
	$(CC) $(_CFLAGS) $^ -o $@ $(_LDFLAGS)
	#$(CC) $(_CFLAGS) -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined -shared $^ -o $@ $(FLUX_CORE_LIBS) -ldl

dyad.o: dyad.c
	echo CFLAGS="$(_CFLAGS)"
	$(CC) $(_CFLAGS) $^ -c -o $@

read_all.o: read_all.c
	$(CC) $(_CFLAGS) $^ -c -o $@

start: dyad.so
	flux module load ./dyad.so $(shell readlink -f ../wrapper/test)/prod


.PHONY: clean install

install: dyad.so
	install -d $(PREFIX)/lib
	install -m 640 dyad.so $(PREFIX)/lib

clean:
	@rm -f *.o *.so
	+$(MAKE) clean -C test
