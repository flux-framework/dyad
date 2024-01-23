#ifndef DYAD_DTL_FLUX_H
#define DYAD_DTL_FLUX_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <stdlib.h>

#include <dyad/dtl/dyad_dtl_impl.h>

struct dyad_dtl_flux {
    flux_t* h;
    dyad_dtl_comm_mode_t comm_mode;
    bool debug;
    flux_future_t* f;
    flux_msg_t* msg;
};

typedef struct dyad_dtl_flux dyad_dtl_flux_t;

dyad_rc_t dyad_dtl_flux_init (const dyad_ctx_t* ctx,
                              dyad_dtl_mode_t mode,
                              dyad_dtl_comm_mode_t comm_mode,
                              bool debug);

dyad_rc_t dyad_dtl_flux_rpc_pack (const dyad_ctx_t* ctx,
                                  const char* restrict upath,
                                  uint32_t producer_rank,
                                  json_t** restrict packed_obj);

dyad_rc_t dyad_dtl_flux_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath);

dyad_rc_t dyad_dtl_flux_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg);

dyad_rc_t dyad_dtl_flux_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f);

dyad_rc_t dyad_dtl_flux_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf);

dyad_rc_t dyad_dtl_flux_return_buffer (const dyad_ctx_t* ctx, void** data_buf);

dyad_rc_t dyad_dtl_flux_establish_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_flux_send (const dyad_ctx_t* ctx, void* buf, size_t buflen);

dyad_rc_t dyad_dtl_flux_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen);

dyad_rc_t dyad_dtl_flux_close_connection (const dyad_ctx_t* ctx);

dyad_rc_t dyad_dtl_flux_finalize (const dyad_ctx_t* ctx);

#endif /* DYAD_DTL_FLUX_H */
