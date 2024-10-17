#ifndef DYAD_COMMON_DYAD_DTL_H
#define DYAD_COMMON_DYAD_DTL_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifdef __cplusplus
extern "C" {
#endif


enum dyad_dtl_mode { DYAD_DTL_UCX = 0,
                     DYAD_DTL_FLUX_RPC = 1,
                     DYAD_DTL_DEFAULT = 1,
                     DYAD_DTL_END = 2 };
typedef enum dyad_dtl_mode dyad_dtl_mode_t;

static const char* dyad_dtl_mode_name[DYAD_DTL_END+1] __attribute__((unused))
    = {"UCX", "FLUX_RPC", "DTL_UNKNOWN"};

enum dyad_dtl_comm_mode {
    DYAD_COMM_NONE = 0,  // Sanity check value for when
                         // connection isn't established
    DYAD_COMM_RECV = 1,  // DTL connection will only receive data
    DYAD_COMM_SEND = 2,  // DTL connection will only send data
    DYAD_COMM_END = 3
};
typedef enum dyad_dtl_comm_mode dyad_dtl_comm_mode_t;


#define DYAD_DTL_RPC_NAME "dyad.fetch"

struct dyad_dtl;

#ifdef __cplusplus
}
#endif

#endif /* DYAD_COMMON_DYAD_DTL_H */
