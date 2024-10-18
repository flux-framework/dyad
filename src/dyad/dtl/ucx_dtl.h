#ifndef DYAD_DTL_UCX_H
#define DYAD_DTL_UCX_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <stdlib.h>
#include <ucp/api/ucp.h>

#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/dtl/ucx_ep_cache.h>

struct dyad_dtl_ucx {
    flux_t* h;
    dyad_dtl_comm_mode_t comm_mode;
    bool debug;
    ucp_context_h ucx_ctx;
    ucp_worker_h ucx_worker;
    ucp_mem_h mem_handle;
    void* net_buf;
    size_t max_transfer_size;
    ucp_address_t* local_address;
    size_t local_addr_len;
    ucp_address_t* remote_address;
    size_t remote_addr_len;
    ucp_ep_h ep;
    ucp_tag_t comm_tag;
    ucx_ep_cache_h ep_cache;
    uint64_t consumer_conn_key;
    // Required for RMA
    void* rkey_buf;
    size_t rkey_size;
    uint64_t cons_buf_ptr;
    // Internal for Sender
    ucp_rkey_h rkey;
};

typedef struct dyad_dtl_ucx dyad_dtl_ucx_t;

dyad_rc_t dyad_dtl_ucx_init (const dyad_ctx_t* ctx,
                             dyad_dtl_mode_t mode,
                             dyad_dtl_comm_mode_t comm_mode,
                             bool debug);

dyad_rc_t dyad_dtl_ucx_rpc_pack (const dyad_ctx_t* ctx,
                                 const char* upath,
                                 uint32_t producer_rank,
                                 json_t** packed_obj);

dyad_rc_t dyad_dtl_ucx_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath);

dyad_rc_t dyad_dtl_ucx_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg);

dyad_rc_t dyad_dtl_ucx_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f);

dyad_rc_t dyad_dtl_ucx_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf);

dyad_rc_t dyad_dtl_ucx_return_buffer (const dyad_ctx_t* ctx, void** data_buf);

dyad_rc_t dyad_dtl_ucx_establish_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_ucx_send (const dyad_ctx_t* ctx, void* buf, size_t buflen);

dyad_rc_t dyad_dtl_ucx_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen);

dyad_rc_t dyad_dtl_ucx_close_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_ucx_finalize (const dyad_ctx_t* ctx);

#endif /* DYAD_DTL_UCX_H */
