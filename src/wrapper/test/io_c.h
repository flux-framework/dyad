#ifndef IO_C_H
#define IO_C_H

#if defined(__cplusplus)
extern "C" {
#endif

int mkdir_of_path (const char* path);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_prod_io (const char* pf, const size_t sz);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_prod_io_buf (const char* pf, const size_t sz);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_cons_io (const char* pf, const size_t sz, int verify);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_cons_io_buf (const char* pf, const size_t sz, int verify);

#if defined(__cplusplus)
};
#endif

#endif // IO_C_H
