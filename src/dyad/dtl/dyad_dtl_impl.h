#ifndef DYAD_DTL_DYAD_DTL_IMPL_H
#define DYAD_DTL_DYAD_DTL_IMPL_H

#include <dyad/common/dyad_flux_log.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/perf/dyad_perf.h>
#include <dyad/dtl/dyad_dtl.h>
#include <flux/core.h>
#include <jansson.h>
#ifdef __cplusplus
#include <cstdint>

extern "C" {
#else
#include <stdint.h>
#endif

#define DYAD_DTL_RPC_NAME "dyad.fetch"

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
    dyad_dtl_private_t private;
    dyad_dtl_mode_t mode;
    dyad_perf_t* perf_handle;
    dyad_rc_t (*rpc_pack) (struct dyad_dtl* restrict self,
                           const char* restrict upath,
                           uint32_t producer_rank,
                           json_t** restrict packed_obj);
    dyad_rc_t (*rpc_unpack) (struct dyad_dtl* self, const flux_msg_t* packed_obj, char** upath);
    dyad_rc_t (*rpc_respond) (struct dyad_dtl* self, const flux_msg_t* orig_msg);
    dyad_rc_t (*rpc_recv_response) (struct dyad_dtl* self, flux_future_t* f);
    dyad_rc_t (*get_buffer) (struct dyad_dtl* self, size_t data_size, void** data_buf);
    dyad_rc_t (*return_buffer) (struct dyad_dtl* self, void** data_buf);
    dyad_rc_t (*establish_connection) (struct dyad_dtl* self);
    dyad_rc_t (*send) (struct dyad_dtl* self, void* buf, size_t buflen);
    dyad_rc_t (*recv) (struct dyad_dtl* self, void** buf, size_t* buflen);
    dyad_rc_t (*close_connection) (struct dyad_dtl* self);
};
typedef struct dyad_dtl dyad_dtl_t;

dyad_rc_t dyad_dtl_init (dyad_dtl_t** dtl_handle,
                         dyad_dtl_mode_t mode,
                         dyad_dtl_comm_mode_t comm_mode,
                         flux_t* h,
                         bool debug,
                         dyad_perf_t* perf_handle);

dyad_rc_t dyad_dtl_finalize (dyad_dtl_t** dtl_handle);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_DYAD_DTL_IMPL_H */
