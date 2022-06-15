ifeq ($(DYAD_SRC_DIR),)
    DYAD_SRC_DIR = ../..
endif

CONFIG = ${DYAD_SRC_DIR}/dyad.cfg
include ${CONFIG}

ifeq ($(MPICXX),)
    MPICXX = mpicxx
endif

VER=${C_VER} -D_POSIX_C_SOURCE=200112L
CXX_VER=-std=c++11

SHARED_SRC_DIR=${DYAD_SRC_DIR}/wrapper/test

_CXXFLAGS = $(CXXFLAGS) $(CXX_VER) $(DYAD_OPTIONS) -I. \
           -I$(SHARED_SRC_DIR) \
           -I${DYAD_SRC_DIR}/common \
           -I${DYAD_SRC_DIR}/lib \
           -I$(FLUX_CORE_INCLUDES) -I$(DYAD_SRC_DIR)/modules

_LDFLAGS = $(LDFLAGS) -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined \
          -L$(FLUX_CORE_LIBPATH) \
          $(FLUX_CORE_LIBS) -ldl $(DYAD_SRC_DIR)/modules/read_all.o #-lstdc++fs

all: test_urpc

test_urpc: test_urpc.cpp $(DYAD_SRC_DIR)/lib/urpc_client.h
	$(CXX) $(_CXXFLAGS) $< -o $@ $(_LDFLAGS) -L$(DYAD_SRC_DIR)/lib -Wl,-rpath=$(DYAD_SRC_DIR)/lib -lurpc_client


.PHONY: clean


run: test_urpc
	./test_urpc 'hostnamer'

clean:
	@rm -f *.o
	@rm -f *.core
	@rm -f test_urpc
