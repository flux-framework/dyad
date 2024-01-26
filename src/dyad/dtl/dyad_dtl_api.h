#ifndef DYAD_DTL_DYAD_DTL_IMPL_H
#define DYAD_DTL_DYAD_DTL_IMPL_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <jansson.h>
#include <sys/types.h>
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_rc.h>
#include <flux/core.h>
#ifdef __cplusplus
#include <cstdint>

extern "C" {
#else
#include <stdint.h>
#endif


// Forward declarations of DTL contexts for the underlying implementations
struct dyad_dtl_ucx;
struct dyad_dtl_flux;

// Union type to store the underlying DTL contexts
union dyad_dtl_private {
    struct dyad_dtl_ucx* ucx_dtl_handle;
    struct dyad_dtl_flux* flux_dtl_handle;
};
typedef union dyad_dtl_private dyad_dtl_private_t;

enum dyad_dtl_comm_mode {
    DYAD_COMM_NONE = 0,  // Sanity check value for when
                         // connection isn't established
    DYAD_COMM_RECV = 1,  // DTL connection will only receive data
    DYAD_COMM_SEND = 2,  // DTL connection will only send data
    DYAD_COMM_END = 3
};
typedef enum dyad_dtl_comm_mode dyad_dtl_comm_mode_t;

struct dyad_dtl {
    dyad_dtl_private_t private_dtl;
    dyad_dtl_mode_t mode;
    dyad_rc_t (*rpc_pack) (const dyad_ctx_t* ctx,
                           const char*  upath,
                           uint32_t producer_rank,
                           json_t**  packed_obj);
    dyad_rc_t (*rpc_unpack) (const dyad_ctx_t* ctx, const flux_msg_t* packed_obj, char** upath);
    dyad_rc_t (*rpc_respond) (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg);
    dyad_rc_t (*rpc_recv_response) (const dyad_ctx_t* ctx, flux_future_t* f);
    dyad_rc_t (*get_buffer) (const dyad_ctx_t* ctx, size_t data_size, void** data_buf);
    dyad_rc_t (*return_buffer) (const dyad_ctx_t* ctx, void** data_buf);
    dyad_rc_t (*establish_connection) (const dyad_ctx_t* ctx);
    dyad_rc_t (*send) (const dyad_ctx_t* ctx, void* buf, size_t buflen);
    dyad_rc_t (*recv) (const dyad_ctx_t* ctx, void** buf, size_t* buflen);
    dyad_rc_t (*send_rma) (const dyad_ctx_t* ctx, void* buf, size_t buflen);
    dyad_rc_t (*recv_rma) (const dyad_ctx_t* ctx, void** buf, size_t* buflen);
    dyad_rc_t (*close_connection) (const dyad_ctx_t* ctx);
};
typedef struct dyad_dtl dyad_dtl_t;

dyad_rc_t dyad_dtl_init (dyad_ctx_t* ctx,
                         dyad_dtl_mode_t mode,
                         dyad_dtl_comm_mode_t comm_mode,
                         bool debug);

dyad_rc_t dyad_dtl_finalize (dyad_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_DYAD_DTL_IMPL_H */
