#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <functional>
#include <new>
#include <unordered_map>
#include <utility>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/common/dyad_structures.h>
#include <dyad/dtl/ucx_ep_cache.h>

using key_type = uint64_t;
using cache_type = std::unordered_map<key_type, ucp_ep_h>;

static void __attribute__ ((unused))
dyad_ucx_ep_err_handler (void* arg, ucp_ep_h ep, ucs_status_t status)
{
    DYAD_C_FUNCTION_START ();
    dyad_ctx_t __attribute__ ((unused))* ctx = (dyad_ctx_t*)arg;
    DYAD_LOG_ERROR (ctx, "An error occured on the UCP endpoint (status = %d)", status);
    DYAD_C_FUNCTION_END ();
}

dyad_rc_t ucx_connect (const dyad_ctx_t* ctx,
                       ucp_worker_h worker,
                       const ucp_address_t* addr,
                       ucp_ep_h* ep)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    ucp_ep_params_t params;
    ucs_status_t status = UCS_OK;
    params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    params.address = addr;
    status = ucp_ep_create (worker, &params, ep);
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_ep_create failed with status %d", (int)status);
        rc = DYAD_RC_UCXCOMM_FAIL;
        goto ucx_connect_done;
    }
    if (*ep == NULL) {
        DYAD_LOG_ERROR (ctx, "ucp_ep_create succeeded, but returned a NULL endpoint");
        rc = DYAD_RC_UCXCOMM_FAIL;
        goto ucx_connect_done;
    }
ucx_connect_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t ucx_disconnect (const dyad_ctx_t* ctx, ucp_worker_h worker, ucp_ep_h ep)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    ucs_status_t status = UCS_OK;
    ucs_status_ptr_t stat_ptr;
    if (ep != NULL) {
        // ucp_tag_send_sync_nbx is the prefered version of this send
        // since UCX 1.9 However, some systems (e.g., Lassen) may have
        // an older verison This conditional compilation will use
        // ucp_tag_send_sync_nbx if using UCX 1.9+, and it will use the
        // deprecated ucp_tag_send_sync_nb if using UCX < 1.9.
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
        ucp_request_param_t close_params;
        close_params.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
        close_params.flags = UCP_EP_CLOSE_FLAG_FORCE;
        stat_ptr = ucp_ep_close_nbx (ep, &close_params);
#else
        // TODO change to FORCE if we decide to enable err handleing
        // mode
        stat_ptr = ucp_ep_close_nb (ep, UCP_EP_CLOSE_MODE_FORCE);
#endif
        // Don't use dyad_ucx_request_wait here because ep_close behaves
        // differently than other UCX calls
        if (stat_ptr != NULL) {
            // Endpoint close is in-progress.
            // Wait until finished
            if (UCS_PTR_IS_PTR (stat_ptr)) {
                do {
                    ucp_worker_progress (worker);
                    status = ucp_request_check_status (stat_ptr);
                } while (status == UCS_INPROGRESS);
                ucp_request_free (stat_ptr);
            }
            // An error occurred during endpoint closure
            // However, the endpoint can no longer be used
            // Get the status code for reporting
            else {
                status = UCS_PTR_STATUS (stat_ptr);
            }
            if (UCX_STATUS_FAIL (status)) {
                rc = DYAD_RC_UCXEP_FAIL;
                goto ucx_disconnect_region_finish;
            }
        }
    }
ucx_disconnect_region_finish:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_init (const dyad_ctx_t* ctx, ucx_ep_cache_h* cache)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (cache == nullptr || *cache != nullptr) {
        rc = DYAD_RC_BADBUF;
        goto ucx_ep_cache_init_done;
    }
    *cache = reinterpret_cast<ucx_ep_cache_h> (new (std::nothrow) cache_type ());
    if (*cache == nullptr) {
        rc = DYAD_RC_SYSFAIL;
        goto ucx_ep_cache_init_done;
    }
ucx_ep_cache_init_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_find (const dyad_ctx_t* ctx,
                                  const ucx_ep_cache_h cache,
                                  const ucp_address_t* addr,
                                  const size_t addr_size,
                                  ucp_ep_h* ep)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (ep == nullptr || *ep != nullptr) {
        rc = DYAD_RC_BADBUF;
        goto ucx_ep_cache_find_done;
    }
    try {
        const auto* cpp_cache = reinterpret_cast<const cache_type*> (cache);
        auto key = ctx->dtl_handle->private_dtl.ucx_dtl_handle->consumer_conn_key;
        auto cache_it = cpp_cache->find (key);
        if (cache_it == cpp_cache->cend ()) {
            *ep = nullptr;
            rc = DYAD_RC_NOTFOUND;
        } else {
            *ep = cache_it->second;
            rc = DYAD_RC_OK;
        }
    } catch (...) {
        *ep = nullptr;
        rc = DYAD_RC_SYSFAIL;
    }
ucx_ep_cache_find_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_insert (const dyad_ctx_t* ctx,
                                    ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    try {
        cache_type* cpp_cache = reinterpret_cast<cache_type*> (cache);
        uint64_t key = ctx->dtl_handle->private_dtl.ucx_dtl_handle->consumer_conn_key;
        DYAD_C_FUNCTION_UPDATE_INT ("cons_key",
                                    ctx->dtl_handle->private_dtl.ucx_dtl_handle->consumer_conn_key)
        auto cache_it = cpp_cache->find (key);
        if (cache_it != cpp_cache->end ()) {
            rc = DYAD_RC_OK;
        } else {
            DYAD_LOG_INFO (ctx, "No cache entry found. Creating new connection");
            rc = ucx_connect (ctx, worker, addr, &ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep);
            if (!DYAD_IS_ERROR (rc)) {
                cpp_cache->insert_or_assign (key, ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep);
                rc = DYAD_RC_OK;
            }
        }
    } catch (...) {
        rc = DYAD_RC_SYSFAIL;
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

static inline cache_type::iterator cache_remove_impl (const dyad_ctx_t* ctx,
                                                      cache_type* cache,
                                                      cache_type::iterator it,
                                                      ucp_worker_h worker)
{
    DYAD_C_FUNCTION_START ();
    if (it != cache->end ()) {
        ucx_disconnect (ctx, worker, it->second);
        // The UCP address was allocated with 'malloc' while unpacking
        // the RPC message. So, we extract it from the key and free
        // it after erasing the iterator
        auto next_it = cache->erase (it);
        DYAD_C_FUNCTION_END ();
        return next_it;
    }
    DYAD_C_FUNCTION_END ();
    return cache->end ();
}

dyad_rc_t dyad_ucx_ep_cache_remove (const dyad_ctx_t* ctx,
                                    ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    try {
        cache_type* cpp_cache = reinterpret_cast<cache_type*> (cache);
        auto key = ctx->dtl_handle->private_dtl.ucx_dtl_handle->consumer_conn_key;
        cache_type::iterator cache_it = cpp_cache->find (key);
        cache_remove_impl (ctx, cpp_cache, cache_it, worker);
        rc = DYAD_RC_OK;
    } catch (...) {
        rc = DYAD_RC_SYSFAIL;
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_finalize (const dyad_ctx_t* ctx,
                                      ucx_ep_cache_h* cache,
                                      ucp_worker_h worker)
{
    DYAD_C_FUNCTION_START ();
    if (cache == nullptr || *cache == nullptr) {
        return DYAD_RC_OK;
    }
    cache_type* cpp_cache = reinterpret_cast<cache_type*> (*cache);
    for (cache_type::iterator it = cpp_cache->begin (); it != cpp_cache->end ();) {
        it = cache_remove_impl (ctx, cpp_cache, it, worker);
    }
    delete cpp_cache;
    *cache = nullptr;
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}
