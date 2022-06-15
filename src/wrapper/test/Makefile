ifeq ($(DYAD_SRC_DIR),)
    DYAD_SRC_DIR = ../..
endif

CONFIG = ${DYAD_SRC_DIR}/dyad.cfg
include ${CONFIG}

ifeq ($(MPICXX),)
    MPICXX = mpicxx
endif

#C_VER=-std=c11
C_VER=-std=c99
CXX_VER=-std=c++11
#CXX_VER=-std=c++98
CXX11_ABI=-D_GLIBCXX_USE_CXX11_ABI=0

CFLAGS += -D_POSIX_C_SOURCE=200112L $(DYAD_OPTIONS) -I./
CXXFLAGS += -D_POSIX_C_SOURCE=200112L $(DYAD_OPTIONS) -I./ $(CXX_VER) $(CXX11_ABI) #-D_TEST_CTOR_
LDFLAGS = -L${DYAD_SRC_DIR}/common/libtap -ltap $(PERFFLOW_LIBS)

all: consumer_test producer_test cons_test prod_test check_file
#all: consumer_test producer_test cons_test prod_test check_file

consumer_test: consumer_test.o ${DYAD_SRC_DIR}/common/utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

consumer_test.o: consumer_test.c
	$(CC)  $(CFLAGS) $< -c

producer_test: producer_test.o ${DYAD_SRC_DIR}/common/utils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

producer_test.o: producer_test.c
	$(CC) $(CFLAGS) $< -c


cons_test: cons_test.o io_test.o flist.o io_c.o ${DYAD_SRC_DIR}/common/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

cons_test.o: cons_test.cpp io_test.o io_c.o flist.hpp
	$(CXX) $(CXXFLAGS) $< -c

prod_test: prod_test.o io_test.o flist.o io_c.o ${DYAD_SRC_DIR}/common/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

prod_test.o: prod_test.cpp io_test.o io_c.o flist.hpp
	$(CXX) $(CXXFLAGS) $< -c

combined_test: combined_test.cpp io_test.o flist.o io_c.o ${DYAD_SRC_DIR}/common/utils.o
	$(MPICXX) $(CXXFLAGS) $^ -o $@ ${LDFLAGS}

io_test.o: io_test.cpp io_test.hpp flist.hpp timer.hpp io_c.h ${DYAD_SRC_DIR}/common/utils.h
	$(CXX) $(CXXFLAGS) $< -c

check_file: check_file.cpp flist.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

flist.o: flist.cpp flist.hpp
	$(CXX) $(CXXFLAGS) $< -c
#$(CXX) $(CXXFLAGS) -fno-var-tracking-assignments $< -c

io_c.o: io_c.c io_c.h ${DYAD_SRC_DIR}/common/utils.h
	$(CC) $(C_VER) $(CFLAGS) $< -c

glob.o: glob.cpp glob.hpp
	$(CXX) $(CXXFLAGS) $< -c


.PHONY: clean

clean:
	@rm -f *.o *.so *breakpoint* *.core
	@rm -f combined_test prod_test cons_test
	@rm -f producer_test consumer_test
	@rm -f check_file
