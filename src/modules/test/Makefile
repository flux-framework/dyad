CONFIG = ../../dyad.cfg
include ${CONFIG}

all: compute

CFLAGS = $(DYAD_OPTIONS) -I$(FLUX_CORE_INCLUDES) -I../../common

compute: compute.c ../../common/utils.o
	$(CC) $(CFLAGS) $^ -o $@ -L$(FLUX_CORE_LIBPATH) $(FLUX_CORE_LIBS) -Wl,-rpath=$(FLUX_CORE_LIBPATH)

.PHONY: clean

clean:
	@rm -f compute
	@rm -f *.o *.so
	@rm -rf debug_consumer.txt cons/
