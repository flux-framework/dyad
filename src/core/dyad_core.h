#ifndef __DYAD_CORE_CLIENT_H__
#define __DYAD_CORE_CLIENT_H__

#include <flux/core.h>

#include "dyad_err.h"

#ifdef __cplusplus
#include <cstdlib>
#include <cstdio>
#else
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#endif

/*****************************************************************************
 *                                                                           *
 *                          DYAD Macro Definitions                           *
 *                                                                           *
 *****************************************************************************/

#define DYAD_PATH_PROD_ENV "DYAD_PATH_PRODUCER"
#define DYAD_PATH_CONS_ENV "DYAD_PATH_CONSUMER"
#define DYAD_CHECK_ENV     "DYAD_SYNC_HEALTH"
// Now defined in src/utils/utils.h  
//#define DYAD_PATH_DELIM    "/"     
                                     
// Debug message                     
#ifndef DPRINTF                      
#define DPRINTF(curr_dyad_ctx, fmt,...) do { \
    if (curr_dyad_ctx && curr_dyad_ctx->debug) fprintf (stderr, fmt, ##__VA_ARGS__); \
} while (0)
#endif // DPRINTF

#define TIME_DIFF(Tstart, Tend ) \
    ((double) (1000000000L * ((Tend).tv_sec  - (Tstart).tv_sec) + \
                              (Tend).tv_nsec - (Tstart).tv_nsec) / 1000000000L)

// Detailed information message that can be omitted
#if DYAD_FULL_DEBUG                       
#define IPRINTF DPRINTF                   
#define IPRINTF_DEFINED                   
#else                                     
#define IPRINTF(curr_dyad_ctx, fmt,...)                  
#endif // DYAD_FULL_DEBUG                 

#if !defined(DYAD_LOGGING_ON) || (DYAD_LOGGING_ON == 0)
#define FLUX_LOG_INFO(curr_dyad_ctx, ...) do {} while (0)
#define FLUX_LOG_ERR(curr_dyad_ctx, ...) do {} while (0)
#else
#define FLUX_LOG_INFO(curr_dyad_ctx, ...) flux_log (\
    curr_dyad_ctx->h, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(curr_dyad_ctx, ...) flux_log_error (\
    curr_dyad_ctxctx->h, __VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @struct dyad_ctx
 */
struct dyad_ctx {
    flux_t *h; // the Flux handle for DYAD 
    bool debug; // if true, perform debug logging
    bool check; // if true, perform some check logging
    bool reenter; // if false, do not recursively enter DYAD
    bool initialized; // if true, DYAD is initialized
    bool shared_storage; // if true, the managed path is shared
    bool sync_started; // TODO             
    unsigned int key_depth; // Depth of bins for the Flux KVS
    unsigned int key_bins; // Number of bins for the Flux KVS
    uint32_t rank; // Flux rank for DYAD   
    char *kvs_namespace; // Flux KVS namespace for DYAD
    char *prod_managed_path; // producer path managed by DYAD
    char *cons_managed_path; // consumer path managed by DYAD
    bool intercept; // if true, intercepts (f)open and (f)close
};
extern const struct dyad_ctx dyad_ctx_default;
typedef struct dyad_ctx dyad_ctx_t;

struct dyad_kvs_response {                 
    char *fpath;                     
    uint32_t owner_rank; 
};                                         
typedef struct dyad_kvs_response dyad_kvs_response_t;
                                           
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
 * @return An integer error code (values TBD)
 */                                       
int dyad_init(bool debug, bool check, bool shared_storage,
        unsigned int key_depth, unsigned int key_bins, 
        const char *kvs_namespace, const char* prod_managed_path,
        const char *cons_managed_path, bool intercept, dyad_ctx_t **ctx);
                                          
/**
 * @brief Wrapper function that performs all the common tasks needed
 *        of a producer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "produced"
 *
 * @return An integer error code (values TBD)
 */
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_produce(dyad_ctx_t *ctx, const char *fname);

/**
 * @brief Wrapper function that performs all the common tasks needed
 *        of a consumer
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being "consumed"
 *
 * @return An integer error code (values TBD)
 */
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_consume(dyad_ctx_t *ctx, const char *fname);

/**
 * @brief Commits (i.e., writes) the file's metadata to the Flux KVS
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being committed
 *
 * @return An integer error code (values TBD)
 */
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_commit(dyad_ctx_t *ctx, const char *fname);

/**
 * @brief Fetches (i.e., reads) the file's metadata from the Flux KVS
 * @param[in] ctx    the DYAD context for the operation
 * @param[in] fname  the name of the file being fetched
 * @param[out] resp  the data fetched from the KVS
 *
 * @return An integer error code (values TBD)
 */
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_fetch(dyad_ctx_t *ctx, const char* fname, 
        dyad_kvs_response_t **resp);
                                           
/**                                        
 * @brief Pulls the file from the producer using Flux RPC
 * @param[in] ctx       the DYAD context for the operation
 * @param[in] fname     the name of the file being pulled
 * @param[in] kvs_data  the data previously fetched from the KVS for
 *                      this file          
 *                                   
 * @return An integer error code (values TBD)
 */                                  
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_pull(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t *kvs_data);    
                                           
/**                                        
 * @brief Frees/Deallocates a block of data fetched from the KVS
 * @param[in] resp  the data previously fetched from the KVS
 *                                         
 * @return An integer error code (values TBD)
 */
int dyad_free_kvs_response(dyad_kvs_response_t *resp);

/**
 * @brief Finalizes the DYAD instance and deallocates the context
 * @param[in] ctx  the DYAD context being finalized
 *
 * @return An integer error code (values TBD)
 */
int dyad_finalize(dyad_ctx_t *ctx);

#if DYAD_SYNC_DIR
int dyad_sync_directory(dyad_ctx_t *ctx, const char *path);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DYAD_CORE_CLIENT_H__ */
