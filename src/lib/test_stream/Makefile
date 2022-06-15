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

CXXFLAGS = $(CXX_VER) $(DYAD_OPTIONS) -I. \
           -I$(SHARED_SRC_DIR) \
           -I${DYAD_SRC_DIR}/common \
           -I${DYAD_SRC_DIR}/lib \
           -I$(FLUX_CORE_INCLUDES)

LDFLAGS = -Wl,-rpath=$(FLUX_CORE_LIBPATH),--no-undefined \
          -L$(FLUX_CORE_LIBPATH) \
          $(FLUX_CORE_LIBS) -ldl #-lstdc++fs

all: test_stream combined_test

test_stream: test_stream.cpp ${DYAD_SRC_DIR}/lib/dyad_stream_api.hpp
	$(CXX) $(CXXFLAGS) $< -o $@ ${LDFLAGS} -L${DYAD_SRC_DIR}/lib -Wl,-rpath=${DYAD_SRC_DIR}/lib -ldyad_fstream


combined_test: combined_test.cpp io_test.o flist.o io_cpp.o ${DYAD_SRC_DIR}/common/utils.o
	$(MPICXX) $(CXXFLAGS) $^ -o $@ ${LDFLAGS} -L${DYAD_SRC_DIR}/lib -Wl,-rpath=${DYAD_SRC_DIR}/lib -ldyad_fstream

io_test.o: io_test.cpp io_test.hpp $(SHARED_SRC_DIR)/flist.hpp $(SHARED_SRC_DIR)/timer.hpp io_cpp.hpp ${DYAD_SRC_DIR}/common/utils.h
	$(CXX) $(CXXFLAGS) $< -c

check_file: check_file.cpp flist.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

flist.o: $(SHARED_SRC_DIR)/flist.cpp $(SHARED_SRC_DIR)/flist.hpp
	$(CXX) $(CXXFLAGS) $< -c
#$(CXX) $(CXXFLAGS) -fno-var-tracking-assignments $< -c

io_cpp.o: io_cpp.cpp io_cpp.hpp ${DYAD_SRC_DIR}/common/utils.h
	$(CXX) $(CXXFLAGS) $< -c

.PHONY: clean


run: test_stream
	./test_stream

clean:
	@rm -f *.o
	@rm -f combined_test
	@rm -f test_stream
	@rm -f test.txt test2.txt
