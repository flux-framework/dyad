#ifndef DYAD_DTL_UCX_EP_CACHE_H
#define DYAD_DTL_UCX_EP_CACHE_H

#include <dyad/common/dyad_rc.h>
#include <dyad/perf/dyad_perf.h>
#include <ucp/api/ucp.h>
#include <flux/core.h>

#ifdef __cplusplus
extern "C" {
#endif

// Macro function used to simplify checking the status
// of UCX operations
#define UCX_STATUS_FAIL(status) (status != UCS_OK)

typedef void* ucx_ep_cache_h;

dyad_rc_t ucx_connect (dyad_perf_t* perf_handle,
                       ucp_worker_h worker,
                       const ucp_address_t* addr,
                       flux_t* h,
                       ucp_ep_h* ep);

dyad_rc_t ucx_disconnect (dyad_perf_t* perf_handle, ucp_worker_h worker, ucp_ep_h ep);

// NOTE: in future, add option to configure replacement strategy
dyad_rc_t dyad_ucx_ep_cache_init (ucx_ep_cache_h* cache);

// NOTE: not positive if UCP addresses are 100% unique by worker
dyad_rc_t dyad_ucx_ep_cache_find (const ucx_ep_cache_h cache,
                                  const ucp_address_t* addr,
                                  const size_t addr_size,
                                  ucp_ep_h* ep);

dyad_rc_t dyad_ucx_ep_cache_insert (ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker,
                                    flux_t* h);

dyad_rc_t dyad_ucx_ep_cache_remove (ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker);

dyad_rc_t dyad_ucx_ep_cache_finalize (ucx_ep_cache_h* cache, ucp_worker_h worker);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_UCX_EP_CACHE_H */