#ifndef DYAD_CORE_DYAD_CTX_H
#define DYAD_CORE_DYAD_CTX_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>

#ifdef __cplusplus
extern "C" {
#endif

DYAD_DLL_EXPORTED extern const struct dyad_ctx dyad_ctx_default;

DYAD_DLL_EXPORTED dyad_ctx_t *dyad_ctx_get();
DYAD_DLL_EXPORTED void dyad_ctx_init(dyad_dtl_comm_mode_t dtl_comm_mode,
                                     void *flux_handle);
DYAD_DLL_EXPORTED void dyad_ctx_fini();

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
 * @param[in]  relative_to_managed_path relative path is relative to the managed
 * path Which one of the managed path it will be relative to depends on the
 * context and it could be either of producer or consumer's path. This is only
 * applicable when there is only one of each.
 * @param[in]  dtl_mode_str   DTL mode
 * @param[in]  dtl_comm_mode  DTL comm mode
 * @param[in]  flux_handle    Flux handle (flux_t*). If NULL, it will be
 * obtained via `flux_open()`
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_init(
    bool debug, bool check, bool shared_storage, bool reinit,
    bool async_publish, bool fsync_write, unsigned int key_depth,
    unsigned int key_bins, unsigned int service_mux, const char *kvs_namespace,
    const char *prod_managed_path, const char *cons_managed_path,
    bool relative_to_managed_path, const char *dtl_mode_str,
    const dyad_dtl_comm_mode_t dtl_comm_mode, void *flux_handle);

/**
 * @brief Intialize the DYAD context using environment variables
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t
dyad_init_env(const dyad_dtl_comm_mode_t dtl_comm_mode, void *flux_handle);

/**
 * @brief Reset producer path. Can be used by the module
 * @param[in] producer path string
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_set_prod_path(const char *path);

/**
 * @brief Reset consumer path. Can be used by the module
 * @param[in] consumer path string
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_set_cons_path(const char *path);

/**
 * @brief Reset dtl mode. Can be used by the module
 * @param[in] dtl mode string
 * @param[in] dtl comm mode:
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_set_and_init_dtl_mode(
    const char *dtl_mode_name, dyad_dtl_comm_mode_t dtl_comm_mode);

/**
 * Reset the contents of the ctx to the default values and deallocate
 * internal objects linked. However, do not destroy the ctx object itself.
 * This is needed for wrapper to handle dyad exceptions as the wrapper requires
 * ctx for it's lifetime
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_clear();

/**
 * @brief Finalizes the DYAD instance and deallocates the context
 *
 * @return An error code from dyad_rc.h
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_finalize();

#ifdef __cplusplus
}
#endif

#endif /* DYAD_CORE_DYAD_CTX_H */
