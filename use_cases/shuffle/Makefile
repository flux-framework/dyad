ifeq ($(DYAD_SRC_DIR),)
    DYAD_SRC_DIR = ../../src
endif

DYAD_OPTIONS += -g -O3 -Wall -fPIC -Wno-stringop-truncation -Wno-stringop-overflow

ifeq ($(MPICXX),)
    MPICXX = mpicxx
endif

C_VER=-std=c11
CXX_VER=-std=c++11
CXX11_ABI=-D_GLIBCXX_USE_CXX11_ABI=0

CFLAGS += -D_POSIX_C_SOURCE=200112L $(DYAD_OPTIONS) \
          -I./ -I$(DYAD_SRC_DIR)/common -I$(DYAD_SRC_DIR)/modules
CXXFLAGS += -D_POSIX_C_SOURCE=200112L $(DYAD_OPTIONS) \
            -I./ -I$(DYAD_SRC_DIR)/common -I$(DYAD_SRC_DIR)/modules \
             $(CXX_VER) $(CXX11_ABI)
LDFLAGS = 
HEADERS = $(DYAD_SRC_DIR)/modules/read_all.h \
          $(DYAD_SRC_DIR)/common/dyad.h \
          $(DYAD_SRC_DIR)/common/utils.h
OBJS = worker.o read_all.o utils.o

all: shuffle utils.o read_all.o worker.o

shuffle: shuffle.cpp $(OBJS) $(HEADERS)
	$(MPICXX) $(CXXFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

utils.o: $(DYAD_SRC_DIR)/common/utils.c $(DYAD_SRC_DIR)/common/utils.h $(DYAD_SRC_DIR)/common/dyad.h
	$(CC)  $(CFLAGS) $< -c

read_all.o: $(DYAD_SRC_DIR)/modules/read_all.c $(DYAD_SRC_DIR)/modules/read_all.h
	$(CC)  $(CFLAGS) $< -c

worker.o: worker.cpp worker.hpp
	$(CXX)  $(CXXFLAGS) $< -c


.PHONY: clean

clean:
	@rm -f *.o *.so *breakpoint* *.core
	@rm -f shuffle
