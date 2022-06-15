#ifndef IO_CPP_HPP
#define IO_CPP_HPP
#include <string>

int mkdir_of_path (const char* path);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_prod_io_stream (const std::string& pf, const size_t sz);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_cons_io_stream (const std::string& pf, const size_t sz, const bool verify);


#endif // IO_CPP_HPP
