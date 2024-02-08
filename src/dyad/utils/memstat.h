#ifndef DYAD_UTILS_MEMSTAT_H
#define DYAD_UTILS_MEMSTAT_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int print_mem_status (const char* note, char* buf, unsigned bufsize);
int print_rusage (const char* note, char* buf, unsigned bufsize);
int print_rusage_in_a_line (const char* note, char* buf, unsigned bufsize);

#ifdef __cplusplus
}
#endif

#endif // DYAD_UTILS_MEMSTAT_H
