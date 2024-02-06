#ifndef DYAD_CORE_DYAD_CORE_H
#define DYAD_CORE_DYAD_CORE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <flux/core.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>
#include <dyad/common/dyad_dtl.h>
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

DYAD_DLL_EXPORTED extern const struct dyad_ctx dyad_ctx_default;

struct dyad_metadata {
    char* fpath;
    uint32_t owner_rank;
};
typedef struct dyad_metadata dyad_metadata_t;

// Debug message
#ifndef DPRINTF
#if VA_OPT_SUPPORTED
#define DPRINTF(curr_dyad_ctx, fmt, ...)           \
    do {                                           \
        if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug) \
            fprintf (stderr, (fmt) __VA_OPT__(,) __VA_ARGS__);  \
    } while (0)
#else
#define DPRINTF(curr_dyad_ctx, fmt, ...)           \
    do {                                           \
        if ((curr_dyad_ctx) && (curr_dyad_ctx)->debug) \
            fprintf (stderr, (fmt), ##__VA_ARGS__);  \
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
 * @brief Intialize the DYAD context
 * @param[in]  debug          true if debugging is enabled
 * @param[in]  check          ???
 * @param[in]  shared_storage indicate if the storage associated
 *                            with the path is shared
 * @param[in]  reinit         force reinitialization
 * @param[in]  async_publish  enable asynchronous publish by producer
 * @param[in]  fsync_write    apply fsync after write by producer
 * @param[in]  key_depth      depth of the key hierarchy for the path
 * @param[in]  key_bins       number of bins used in key hashing
 * @param[in]  service_mux    number of brokers sharing node-local storage
 * @param[in]  kvs_namespace  Flux KVS namespace to be used for this
 *                            instance of DYAD
 * @param[out] ctx            the newly initialized context
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_init (bool debug,
                                       bool check,
                                       bool shared_storage,
                                       bool reinit,
                                       bool async_publish,
                                       bool fsync_write,
                                       unsigned int key_depth,
                                       unsigned int key_bins,
                                       unsigned int service_mux,
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
 * @brief Wrapper function that performs all the common tasks needed
 *        of a consumer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "consumed"
 * @param[in] mdata  a dyad_metadata_t object containing the metadata for the file
 *                   User is responsible for deallocating this object
 *
 * @return An error code from dyad_rc.h
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_consume_w_metadata (dyad_ctx_t* ctx, const char* fname,
                                                                       const dyad_metadata_t* mdata);

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
