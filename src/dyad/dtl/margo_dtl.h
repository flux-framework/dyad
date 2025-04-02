#ifndef DYAD_DTL_margo_H
#define DYAD_DTL_margo_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <stdlib.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <margo.h>

struct dyad_dtl_margo {
    flux_t* h;
    bool debug;
    margo_instance_id mid;      // margo id
    hg_addr_t local_addr;       // margo local server address
    hg_addr_t remote_addr;      // margo remote server address
    hg_id_t sendrecv_rpc_id;    // margo rpc id for send/recv
    bool recv_ready;
    size_t recv_len;
    void* recv_buffer;
};

typedef struct dyad_dtl_margo dyad_dtl_margo_t;

dyad_rc_t dyad_dtl_margo_init (const dyad_ctx_t* ctx,
                             dyad_dtl_mode_t mode,
                             dyad_dtl_comm_mode_t comm_mode,
                             bool debug);

dyad_rc_t dyad_dtl_margo_rpc_pack (const dyad_ctx_t* ctx,
                                 const char* upath,
                                 uint32_t producer_rank,
                                 json_t** packed_obj);

dyad_rc_t dyad_dtl_margo_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath);

dyad_rc_t dyad_dtl_margo_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg);

dyad_rc_t dyad_dtl_margo_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f);

dyad_rc_t dyad_dtl_margo_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf);

dyad_rc_t dyad_dtl_margo_return_buffer (const dyad_ctx_t* ctx, void** data_buf);

dyad_rc_t dyad_dtl_margo_establish_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_margo_send (const dyad_ctx_t* ctx, void* buf, size_t buflen);

dyad_rc_t dyad_dtl_margo_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen);

dyad_rc_t dyad_dtl_margo_close_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_margo_finalize (const dyad_ctx_t* ctx);

#endif /* DYAD_DTL_margo_H */
