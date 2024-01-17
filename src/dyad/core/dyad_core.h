#ifndef DYAD_CORE_DYAD_CORE_H
#define DYAD_CORE_DYAD_CORE_H


#include <dyad/common/dyad_rc.h>
#include <dyad/dtl/dyad_dtl.h>
#include <sys/types.h>
#include <unistd.h>
#include <flux/core.h>

#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#endif

/*****************************************************************************
 *                                                                           *
 *                          DYAD Macro Definitions                           *
 *                                                                           *
 *****************************************************************************/

// Now defined in src/utils/utils.h
// #define DYAD_PATH_DELIM    "/"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct dyad_ctx
 */
struct dyad_ctx {
    flux_t* h;                    // the Flux handle for DYAD
    struct dyad_dtl* dtl_handle;  // Opaque handle to DTL info
    bool debug;                   // if true, perform debug logging
    bool check;                   // if true, perform some check logging
    bool reenter;                 // if false, do not recursively enter DYAD
    bool initialized;             // if true, DYAD is initialized
    bool shared_storage;          // if true, the managed path is shared
    unsigned int key_depth;       // Depth of bins for the Flux KVS
    unsigned int key_bins;        // Number of bins for the Flux KVS
    uint32_t rank;                // Flux rank for DYAD
    char* kvs_namespace;          // Flux KVS namespace for DYAD
    char* prod_managed_path;      // producer path managed by DYAD
    char* cons_managed_path;      // consumer path managed by DYAD
};
DYAD_DLL_EXPORTED extern const struct dyad_ctx dyad_ctx_default;
typedef struct dyad_ctx dyad_ctx_t;

struct dyad_metadata {
    char* fpath;
    uint32_t owner_rank;
};
typedef struct dyad_metadata dyad_metadata_t;

// Debug message
#ifndef DPRINTF
#define DPRINTF(curr_dyad_ctx, fmt, ...)           \
    do {                                           \
        if (curr_dyad_ctx && curr_dyad_ctx->debug) \
            fprintf (stderr, fmt, ##__VA_ARGS__);  \
    } while (0)
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
 * @brief Intialize the DYAD context
 * @param[in]  debug          true if debugging is enabled
 * @param[in]  check          ???
 * @param[in]  shared_storage indicate if the storage associated
 *                            with the path is shared
 * @param[in]  key_depth      depth of the key hierarchy for the path
 * @param[in]  key_bins       number of bins used in key hashing
 * @param[in]  kvs_namespace  Flux KVS namespace to be used for this
 *                            instance of DYAD
 * @param[out] ctx            the newly initialized context
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_init (bool debug,
                                       bool check,
                                       bool shared_storage,
                                       unsigned int key_depth,
                                       unsigned int key_bins,
                                       const char* kvs_namespace,
                                       const char* prod_managed_path,
                                       const char* cons_managed_path,
                                       dyad_dtl_mode_t dtl_mode,
                                       dyad_ctx_t** ctx);

/**
 * @brief Intialize the DYAD context using environment variables
 * @param[out] ctx the newly initialized context
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_init_env (dyad_ctx_t** ctx);

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
 * @brief Finalizes the DYAD instance and deallocates the context
 * @param[in] ctx  the DYAD context being finalized
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_finalize (dyad_ctx_t** ctx);

#if DYAD_SYNC_DIR
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED int dyad_sync_directory (dyad_ctx_t* ctx, const char* path);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DYAD_CORE_DYAD_CORE */
