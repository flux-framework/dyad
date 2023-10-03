#ifndef DYAD_DTL_FLUX_H
#define DYAD_DTL_FLUX_H

#include <stdlib.h>

#include <dyad/dtl/dyad_dtl_impl.h>

struct dyad_dtl_flux {
    flux_t* h;
    bool debug;
    flux_future_t* f;
    flux_msg_t* msg;
};

typedef struct dyad_dtl_flux dyad_dtl_flux_t;

dyad_rc_t dyad_dtl_flux_init (dyad_dtl_t* self,
                              dyad_dtl_mode_t mode,
                              flux_t* h,
                              bool debug);

dyad_rc_t dyad_dtl_flux_rpc_pack (dyad_dtl_t* restrict self,
                                  const char* restrict upath,
                                  uint32_t producer_rank,
                                  json_t** restrict packed_obj);

dyad_rc_t dyad_dtl_flux_rpc_unpack (dyad_dtl_t* self,
                                    const flux_msg_t* msg,
                                    char** upath);

dyad_rc_t dyad_dtl_flux_rpc_respond (dyad_dtl_t* self, const flux_msg_t* orig_msg);

dyad_rc_t dyad_dtl_flux_rpc_recv_response (dyad_dtl_t* self, flux_future_t* f);

dyad_rc_t dyad_dtl_flux_establish_connection (dyad_dtl_t* self,
                                              dyad_dtl_comm_mode_t comm_mode);

dyad_rc_t dyad_dtl_flux_send (dyad_dtl_t* self, void* buf, size_t buflen);

dyad_rc_t dyad_dtl_flux_recv (dyad_dtl_t* self, void** buf, size_t* buflen);

dyad_rc_t dyad_dtl_flux_close_connection (dyad_dtl_t* self);

dyad_rc_t dyad_dtl_flux_finalize (dyad_dtl_t** self);

#endif /* DYAD_DTL_FLUX_H */