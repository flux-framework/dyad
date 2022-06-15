CONFIG = ../dyad.cfg
include $(CONFIG)

ifeq ($(DYAD_DEBUG),1)
  DYAD_OPTIONS += -DDYAD_FULL_DEBUG=1
  DYAD_OPTIONS += -DDYAD_LOGGING_ON=1
  $(info DYAD_DEBUG is enabled)
endif


_CFLAGS = $(CFLAGS) $(C_VER) $(DYAD_OPTIONS) -I$(FLUX_CORE_INCLUDES)

all: utils.o murmur3.o
	+$(MAKE) -C libtap

utils.o: utils.c utils.h dyad.h
	$(CC) $(_CFLAGS) -Wall $< -c -o $@

murmur3.o: murmur3.c murmur3.h
	$(CC) $(_CFLAGS) -Wall $< -c -o $@

.PHONY: clean

clean:
	@rm -f *.o
	+$(MAKE) clean -C libtap
