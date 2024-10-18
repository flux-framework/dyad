#ifndef DYAD_DTL_UCX_EP_CACHE_H
#define DYAD_DTL_UCX_EP_CACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <flux/core.h>
#include <ucp/api/ucp.h>

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>
#include <dyad/dtl/ucx_dtl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Macro function used to simplify checking the status
// of UCX operations
#define UCX_STATUS_FAIL(status) (status != UCS_OK)

dyad_rc_t ucx_connect (const dyad_ctx_t* ctx,
                       ucp_worker_h worker,
                       const ucp_address_t* addr,
                       ucp_ep_h* ep);

dyad_rc_t ucx_disconnect (const dyad_ctx_t* ctx, ucp_worker_h worker, ucp_ep_h ep);

// NOTE: in future, add option to configure replacement strategy
dyad_rc_t dyad_ucx_ep_cache_init (const dyad_ctx_t* ctx, ucx_ep_cache_h* cache);

// NOTE: not positive if UCP addresses are 100% unique by worker
dyad_rc_t dyad_ucx_ep_cache_find (const dyad_ctx_t* ctx,
                                  const ucx_ep_cache_h cache,
                                  const ucp_address_t* addr,
                                  const size_t addr_size,
                                  ucp_ep_h* ep);

dyad_rc_t dyad_ucx_ep_cache_insert (const dyad_ctx_t* ctx,
                                    ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker);

dyad_rc_t dyad_ucx_ep_cache_remove (const dyad_ctx_t* ctx,
                                    ucx_ep_cache_h cache,
                                    const ucp_address_t* addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker);

dyad_rc_t dyad_ucx_ep_cache_finalize (const dyad_ctx_t* ctx,
                                      ucx_ep_cache_h* cache,
                                      ucp_worker_h worker);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_UCX_EP_CACHE_H */
