#ifndef DYAD_CORE_DYAD_CORE_H
#define DYAD_CORE_DYAD_CORE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>

#ifdef __cplusplus
extern "C" {
#endif
#if DYAD_PERFFLOW
#define DYAD_CORE_FUNC_MODS __attribute__ ((annotate ("@critical_path()"))) static
#else
#define DYAD_CORE_FUNC_MODS static inline
#endif
DYAD_DLL_EXPORTED extern const struct dyad_ctx dyad_ctx_default;

struct dyad_metadata {
    char* fpath;
    uint32_t owner_rank;
};
typedef struct dyad_metadata dyad_metadata_t;

// Debug message
#ifndef DPRINTF
#if VA_OPT_SUPPORTED
#define DPRINTF(curr_dyad_ctx, fmt, ...)                        \
    do {                                                        \
        if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug)          \
            fprintf (stderr, (fmt)__VA_OPT__ (, ) __VA_ARGS__); \
    } while (0)
#else
#define DPRINTF(curr_dyad_ctx, fmt, ...)               \
    do {                                               \
        if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug) \
            fprintf (stderr, (fmt), ##__VA_ARGS__);    \
    } while (0)
#endif
#endif  // DPRINTF

#define TIME_DIFF(Tstart, Tend)                                                                    \
    ((double)(1000000000L * ((Tend).tv_sec - (Tstart).tv_sec) + (Tend).tv_nsec - (Tstart).tv_nsec) \
     / 1000000000L)

// Detailed information message that can be omitted
#if DYAD_FULL_DEBUG
#define IPRINTF DPRINTF
#define IPRINTF_DEFINED
#else
#define IPRINTF(curr_dyad_ctx, fmt, ...)
#endif  // DYAD_FULL_DEBUG

/**
 * @brief Wrapper function that performs all the common tasks needed
 *        of a producer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "produced"
 *
 * @return An error code from dyad_rc.h
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_produce (dyad_ctx_t* ctx, const char* fname);

/**
 * @brief Obtain DYAD metadata for a file in the consumer-managed directory
 * @param[in]  ctx         the DYAD context for the operation
 * @param[in]  fname       the name of the file for which metadata is obtained
 * @param[in]  should_wait if true, wait for the file to be produced before returning
 * @param[out] mdata       a dyad_metadata_t object containing the metadata for the file
 *
 * @return An error code from dyad_rc.h
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_get_metadata (dyad_ctx_t* ctx,
                                                                 const char* fname,
                                                                 bool should_wait,
                                                                 dyad_metadata_t** mdata);

DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_free_metadata (dyad_metadata_t** mdata);

/**
 * @brief Wrapper function that performs all the common tasks needed
 *        of a consumer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "consumed"
 *
 * @return An error code from dyad_rc.h
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_consume (dyad_ctx_t* ctx, const char* fname);

/**
 * @brief Wrapper function that performs all the common tasks needed
 *        of a consumer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "consumed"
 * @param[in] mdata  a dyad_metadata_t object containing the metadata for the file
 *                   User is responsible for deallocating this object
 *
 * @return An error code from dyad_rc.h
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t
dyad_consume_w_metadata (dyad_ctx_t* ctx, const char* fname, const dyad_metadata_t* mdata);

/**
 * Private Function definitions
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_get_data (const dyad_ctx_t* ctx,
                                           const dyad_metadata_t* mdata,
                                           char** file_data,
                                           size_t* file_len);
DYAD_DLL_EXPORTED dyad_rc_t dyad_commit (dyad_ctx_t* ctx, const char* fname);

DYAD_DLL_EXPORTED int gen_path_key (const char* str,
                                    char* path_key,
                                    const size_t len,
                                    const uint32_t depth,
                                    const uint32_t width);

DYAD_DLL_EXPORTED dyad_rc_t dyad_kvs_read (const dyad_ctx_t* ctx,
                                           const char* topic,
                                           const char* upath,
                                           bool should_wait,
                                           dyad_metadata_t** mdata);

#if DYAD_SYNC_DIR
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED int dyad_sync_directory (dyad_ctx_t* ctx, const char* path);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DYAD_CORE_DYAD_CORE */
