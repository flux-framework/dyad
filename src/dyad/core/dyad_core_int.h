#ifndef DYAD_CORE_DYAD_CORE_INT_H
#define DYAD_CORE_DYAD_CORE_INT_H

// clang-format off
#include <dyad/core/dyad_core.h> 
#include <dyad/common/dyad_structures_int.h>
// clang-format on

// Debug message
#ifndef DPRINTF
#if VA_OPT_SUPPORTED
#define DPRINTF(curr_dyad_ctx, fmt, ...)                                       \
  do {                                                                         \
    if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug)                             \
      fprintf(stderr, (fmt)__VA_OPT__(, ) __VA_ARGS__);                        \
  } while (0)
#else
#define DPRINTF(curr_dyad_ctx, fmt, ...)                                       \
  do {                                                                         \
    if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug)                             \
      fprintf(stderr, (fmt), ##__VA_ARGS__);                                   \
  } while (0)
#endif
#endif // DPRINTF

#define TIME_DIFF(Tstart, Tend)                                                \
  ((double)(1000000000L * ((Tend).tv_sec - (Tstart).tv_sec) + (Tend).tv_nsec - \
            (Tstart).tv_nsec) /                                                \
   1000000000L)

// Detailed information message that can be omitted
#if DYAD_FULL_DEBUG
#define IPRINTF DPRINTF
#define IPRINTF_DEFINED
#else
#define IPRINTF(curr_dyad_ctx, fmt, ...)
#endif // DYAD_FULL_DEBUG

DYAD_DLL_EXPORTED int gen_path_key(const char *str, char *path_key,
                                   const size_t len, const uint32_t depth,
                                   const uint32_t width);

/**
 * Private Function definitions
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_get_data(const dyad_ctx_t *ctx,
                                          const dyad_metadata_t *mdata,
                                          char **file_data, size_t *file_len);
DYAD_DLL_EXPORTED dyad_rc_t dyad_commit(dyad_ctx_t *ctx, const char *fname);

DYAD_DLL_EXPORTED dyad_rc_t dyad_kvs_read(const dyad_ctx_t *ctx,
                                          const char *topic, const char *upath,
                                          bool should_wait,
                                          dyad_metadata_t **mdata);

#if DYAD_SYNC_DIR
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED int dyad_sync_directory(dyad_ctx_t *ctx,
                                                            const char *path);
#endif

#endif /* DYAD_CORE_DYAD_CORE_INT_H */