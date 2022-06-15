#ifndef IO_TEST_H
#define IO_TEST_H

#include <vector>
#include <string>
#include "flist.hpp"
#include "timer.hpp"

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
bool producer (const flist_t& flist, bool buffered_io,
               unsigned usec = 0u);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
bool consumer (const flist_t& flist, bool buffered_io,
               unsigned usec = 0u, bool verify = false);

void mkpath (const std::string& path);
std::string get_dyad_path (char *path);
std::string get_data_path (const std::string& dyad_path,
                           const std::string& context,
                           int iter);

#endif // IO_TEST_H
