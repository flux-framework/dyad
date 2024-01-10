#include <dyad/common/dyad_flux_log.h>
#include <dyad/dtl/ucx_ep_cache.h>

#include <functional>
#include <new>
#include <unordered_map>
#include <utility>

using key_type = std::pair<ucp_address_t*, size_t>;
using cache_type = std::unordered_map<key_type, ucp_ep_h>;

// Adapted from the following code:
// https://github.com/LLNL/wcs/blob/6590f592a69fe0c553ebf27a1bc348ff1fbbd813/src/utils/seed.hpp#L23
namespace std
{
template <>
struct hash<key_type> {
    using arg_t = key_type;
    using result_t = size_t;

    result_t operator() (const arg_t& a) const
    {
        hash<uint8_t> hasher;
        result_t hashed_val = 0ul;
        uint8_t* byte_buf = reinterpret_cast<uint8_t*> (a.first);
        for (size_t i = 0ul; i < a.second; ++i) {
            hashed_val = hashed_val * 31 + hasher (*(byte_buf + i));
        }
        return hashed_val;
    }
};
}  // namespace std

static void dyad_ucx_ep_err_handler (void* arg, ucp_ep_h ep, ucs_status_t status)
{
    flux_t* h = (flux_t*)arg;
    FLUX_LOG_ERR (h, "An error occured on the UCP endpoint (status = %d)\n", status);
}

dyad_rc_t ucx_connect (dyad_perf_t* perf_handle,
                       ucp_worker_h worker,
                       const ucp_address_t* addr,
                       flux_t* h,
                       ucp_ep_h* ep)
{
    ucp_ep_params_t params;
    ucs_status_t status = UCS_OK;
    DYAD_PERF_REGION_BEGIN (perf_handle, "ucx_connect");
    params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE
                        | UCP_EP_PARAM_FIELD_ERR_HANDLER;
    params.address = addr;
    params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
    params.err_handler.cb = dyad_ucx_ep_err_handler;
    params.err_handler.arg = (void*)h;
    status = ucp_ep_create (worker, &params, ep);
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (h, "ucp_ep_create failed with status %d\n", (int)status);
        DYAD_PERF_REGION_END (perf_handle, "ucx_connect");
        return DYAD_RC_UCXCOMM_FAIL;
    }
    if (*ep == NULL) {
        FLUX_LOG_ERR (h, "ucp_ep_create succeeded, but returned a NULL endpoint");
        DYAD_PERF_REGION_END (perf_handle, "ucx_connect");
        return DYAD_RC_UCXCOMM_FAIL;
    }
    DYAD_PERF_REGION_END (perf_handle, "ucx_connect");
    return DYAD_RC_OK;
}

dyad_rc_t ucx_disconnect (dyad_perf_t* perf_handle, ucp_worker_h worker, ucp_ep_h ep)
{
    dyad_rc_t rc = DYAD_RC_OK;
    ucs_status_t status = UCS_OK;
    ucs_status_ptr_t stat_ptr;
    DYAD_PERF_REGION_BEGIN (perf_handle, "ucx_disconnect");
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
    DYAD_PERF_REGION_END (perf_handle, "ucx_disconnect");
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_init (ucx_ep_cache_h* cache)
{
    if (cache == nullptr || *cache != nullptr) {
        return DYAD_RC_BADBUF;
    }
    *cache = reinterpret_cast<ucx_ep_cache_h> (new (std::nothrow) cache_type ());
    if (*cache == nullptr) {
        return DYAD_RC_SYSFAIL;
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_ucx_ep_cache_find (const ucx_ep_cache_h cache,
                                  const ucp_address_t* addr,
                                  const size_t addr_size,
                                  ucp_ep_h* ep)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (ep == nullptr || *ep != nullptr) {
        return DYAD_RC_BADBUF;
    }
    try {
        const cache_type* cpp_cache = reinterpret_cast<const cache_type*> (cache);
        auto key = std::make_pair<ucp_address_t*, size_t> (const_cast<ucp_address_t*> (addr),
                                                           static_cast<size_t> (addr_size));
        cache_type::const_iterator cache_it = cpp_cache->find (key);
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
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_insert (ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker,
                                    flux_t* h)
{
    dyad_rc_t rc = DYAD_RC_OK;
    try {
        cache_type* cpp_cache = reinterpret_cast<cache_type*> (cache);
        auto key = std::make_pair<ucp_address_t*, size_t> (const_cast<ucp_address_t*> (addr),
                                                           static_cast<size_t> (addr_size));
        cache_type::const_iterator cache_it = cpp_cache->find (key);
        if (cache_it != cpp_cache->cend ()) {
            rc = DYAD_RC_OK;
        } else {
            ucp_ep_h ep;
            rc = ucx_connect (nullptr, worker, addr, h, &ep);
            if (!DYAD_IS_ERROR (rc)) {
                cpp_cache->emplace (key, ep);
                rc = DYAD_RC_OK;
            }
        }
    } catch (...) {
        rc = DYAD_RC_SYSFAIL;
    }
    return rc;
}

static inline cache_type::iterator cache_remove_impl (cache_type* cache,
                                                      cache_type::iterator it,
                                                      ucp_worker_h worker)
{
    if (it != cache->end ()) {
        ucx_disconnect (nullptr, worker, it->second);
        // The UCP address was allocated with 'malloc' while unpacking
        // the RPC message. So, we extract it from the key and free
        // it after erasing the iterator
        ucp_address_t* addr = it->first.first;
        auto next_it = cache->erase (it);
        free (addr);
        return next_it;
    }
    return cache->end ();
}

dyad_rc_t dyad_ucx_ep_cache_remove (ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker)
{
    dyad_rc_t rc = DYAD_RC_OK;
    try {
        cache_type* cpp_cache = reinterpret_cast<cache_type*> (cache);
        auto key = std::make_pair<ucp_address_t*, size_t> (const_cast<ucp_address_t*> (addr),
                                                           static_cast<size_t> (addr_size));
        cache_type::iterator cache_it = cpp_cache->find (key);
        cache_remove_impl (cpp_cache, cache_it, worker);
        rc = DYAD_RC_OK;
    } catch (...) {
        rc = DYAD_RC_SYSFAIL;
    }
    return rc;
}

dyad_rc_t dyad_ucx_ep_cache_finalize (ucx_ep_cache_h* cache, ucp_worker_h worker)
{
    if (cache == nullptr || *cache == nullptr) {
        return DYAD_RC_OK;
    }
    cache_type* cpp_cache = reinterpret_cast<cache_type*> (*cache);
    for (cache_type::iterator it = cpp_cache->begin (); it != cpp_cache->end ();) {
        it = cache_remove_impl (cpp_cache, it, worker);
    }
    delete cpp_cache;
    *cache = nullptr;
    return DYAD_RC_OK;
}