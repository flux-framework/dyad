#ifndef DYAD_DTL_UCX_H
#define DYAD_DTL_UCX_H

#include <stdlib.h>
#include <ucp/api/ucp.h>

#include "dyad_dtl_impl.h"

struct dyad_dtl_ucx {
    flux_t* h;
    bool debug;
    ucp_context_h ucx_ctx;
    ucp_worker_h ucx_worker;
    ucp_ep_h ep;
    dyad_dtl_comm_mode_t curr_comm_mode;
    ucp_address_t* consumer_address;
    size_t addr_len;
    ucp_tag_t comm_tag;
};

typedef struct dyad_dtl_ucx dyad_dtl_ucx_t;

dyad_rc_t dyad_dtl_ucx_init (dyad_dtl_t* self,
                             dyad_dtl_mode_t mode,
                             flux_t* h,
                             bool debug);

dyad_rc_t dyad_dtl_ucx_rpc_pack (dyad_dtl_t* restrict self,
                                 const char* restrict upath,
                                 uint32_t producer_rank,
                                 json_t** restrict packed_obj);

dyad_rc_t dyad_dtl_ucx_rpc_unpack (dyad_dtl_t* self, const flux_msg_t* msg, char** upath);

dyad_rc_t dyad_dtl_ucx_rpc_respond (dyad_dtl_t* self, const flux_msg_t* orig_msg);

dyad_rc_t dyad_dtl_ucx_rpc_recv_response (dyad_dtl_t* self, flux_future_t* f);

dyad_rc_t dyad_dtl_ucx_establish_connection (dyad_dtl_t* self,
                                             dyad_dtl_comm_mode_t comm_mode);

dyad_rc_t dyad_dtl_ucx_send (dyad_dtl_t* self, void* buf, size_t buflen);

dyad_rc_t dyad_dtl_ucx_recv (dyad_dtl_t* self, void** buf, size_t* buflen);

dyad_rc_t dyad_dtl_ucx_close_connection (dyad_dtl_t* self);

dyad_rc_t dyad_dtl_ucx_finalize (dyad_dtl_t** self);

#endif /* DYAD_DTL_UCX_H */