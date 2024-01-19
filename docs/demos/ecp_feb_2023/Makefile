DYAD_LIB_PATH = $(DYAD_INSTALL_PREFIX)/lib
DYAD_INCLUDE_PATH = $(DYAD_INSTALL_PREFIX)/include

CFLAGS_LOC = -std=c11 -DDYAD_HAS_CONFIG=1 $(CFLAGS)
CPPFLAGS_LOC = -O3 -I. $(CPPFLAGS)
CXXFLAGS_LOC = -std=c++11 -DDYAD_HAS_CONFIG=1 -I$(DYAD_INCLUDE_PATH) $(CXXFLAGS)
CXXLIBS_LOC = -L$(DYAD_LIB_PATH) -Wl,-rpath=$(DYAD_LIB_PATH) -ldyad_fstream $(CXXLIBS)

all: c_prod c_cons cpp_prod cpp_cons

c_prod: prod.c
	$(CC) $(CPPFLAGS_LOC) $(CFLAGS_LOC) $? -o $@

c_cons: cons.c
	$(CC) $(CPPFLAGS_LOC) $(CFLAGS_LOC) $? -o $@

cpp_prod: prod.cpp
	$(CXX) $(CPPFLAGS_LOC) $(CXXFLAGS_LOC) $? -o $@ $(CXXLIBS_LOC)

cpp_cons: cons.cpp
	$(CXX) $(CPPFLAGS_LOC) $(CXXFLAGS_LOC) $? -o $@ $(CXXLIBS_LOC)

.PHONY: clean

clean:
	rm c_prod c_cons cpp_prod cpp_cons
