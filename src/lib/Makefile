CONFIG = ../dyad.cfg
include $(CONFIG)

ifeq ($(DYAD_DEBUG),1)
  DYAD_OPTIONS += -DDYAD_FULL_DEBUG=1
  DYAD_OPTIONS += -DDYAD_LOGGING_ON=1
  $(info DYAD_DEBUG is enabled)
endif

$(info DYAD_DEBUG="$(DYAD_DEBUG)")
$(info DYAD_OPTIONS="$(DYAD_OPTIONS)")


_CFLAGS = $(CFLAGS) $(C_VER) $(DYAD_OPTIONS) -I./ -I../common -I$(FLUX_CORE_INCLUDES)
_CXXFLAGS = $(CXXFLAGS) $(CXX_VER) $(DYAD_OPTIONS) -I./ -I../common -I$(FLUX_CORE_INCLUDES)
_LDFLAGS = $(LDFLAGS) -L$(FLUX_CORE_LIBPATH) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined -shared $(FLUX_CORE_LIBS) -ldl
#--disable-static

ifeq ($(BUILD_TEST),1)
all: libdyad_fstream.so
	+$(MAKE) -C test_stream
else
all: libdyad_fstream.so
endif

OBJS = dyad_stream_core.o ../common/utils.o ../common/murmur3.o

libdyad_fstream.so: dyad_stream_api.hpp $(OBJS)
	$(CXX) $(_CXXFLAGS) $(OBJS) $(_LDFLAGS) -o $@

dyad_stream_core.o: dyad_stream_core.cpp dyad_stream_core.hpp ../common/utils.h ../common/murmur3.h
	$(CXX) $(_CXXFLAGS) -DDYAD_CHECK=1 $< -c -o $@


.PHONY: clean install

install: libdyad_fstream.so dyad_stream_api.hpp dyad_stream_core.hpp
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include
	install -m 640 libdyad_fstream.so $(PREFIX)/lib
	install -m 640 dyad_stream_api.hpp $(PREFIX)/include
	install -m 640 dyad_stream_core.hpp $(PREFIX)/include
	install -m 640 dyad_params.hpp $(PREFIX)/include

ifeq ($(BUILD_TEST),1)
clean:
	@rm -f *.o *~ libdyad_fstream.so
	+$(MAKE) clean -C test_stream
else
clean:
	@rm -f *.o *~ libdyad_fstream.so
endif
