#ifndef DYAD_DTL_DYAD_RC_H
#define DYAD_DTL_DYAD_RC_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#if BUILDING_DYAD
#define DYAD_DLL_EXPORTED __attribute__ ((__visibility__ ("default")))
#else
#define DYAD_DLL_EXPORTED
#endif

#define DYAD_DLL_HIDDEN __attribute__ ((__visibility__ ("default")))

#if DYAD_PERFFLOW
#define DYAD_PFA_ANNOTATE __attribute__ ((annotate ("@critical_path()")))
#else
#define DYAD_PFA_ANNOTATE
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum dyad_core_return_codes {
    DYAD_RC_OK = 0,                   // Operation worked correctly
    DYAD_RC_SYSFAIL = -1,             // Some sys call or C standard
                                      // library call failed
    // Internal
    DYAD_RC_NOCTX = -1000,            // No DYAD Context found
    DYAD_RC_BADMETADATA = -1001,      // Cannot create/populate a DYAD response
    DYAD_RC_BADFIO = -1002,           // File I/O failed
    DYAD_RC_BADMANAGEDPATH = -1003,   // Cons or Prod Manged Path is bad
    DYAD_RC_BADDTLMODE = -1004,       // Invalid DYAD DTL mode provided
    DYAD_RC_UNTRACKED = -1005,        // Provided path is not tracked by DYAD
    DYAD_RC_BAD_B64DECODE = -1006,    // Decoding of data w/ base64 failed
    DYAD_RC_BAD_COMM_MODE = -1007,    // Invalid comm mode provided to DTL
    DYAD_RC_BAD_CLI_ARG_DEF = -1008,  // Trying to define a CLI argument failed
    DYAD_RC_BAD_CLI_PARSE = -1009,    // Trying to parse CLI arguments failed
    DYAD_RC_BADBUF = -1010,           // Invalid buffer/pointer passed to function


    // FLUX
    DYAD_RC_FLUXFAIL = -2000,          // Some Flux function failed
    DYAD_RC_BADCOMMIT = -2001,         // Flux KVS commit didn't work
    DYAD_RC_NOTFOUND = -2002,          // Flux KVS lookup didn't work
    DYAD_RC_BADFETCH = -2003,          // Flux KVS commit didn't work
    DYAD_RC_BADRPC = -2004,            // Flux RPC pack or get didn't work
    DYAD_RC_BADPACK = -2005,           // JSON packing failed
    DYAD_RC_BADUNPACK = -2006,         // JSON unpacking failed
    DYAD_RC_RPC_FINISHED = -2007,      // The Flux RPC responded with ENODATA (i.e.,
                                       // end of stream) sooner than expected

    //UCX
    DYAD_RC_UCXINIT_FAIL = -3001,     // UCX initialization failed
    DYAD_RC_UCXWAIT_FAIL = -3002,     // UCX wait (either custom or
                                      // 'ucp_worker_wait') failed
    DYAD_RC_UCXEP_FAIL = -3003,       // An operation on a ucp_ep_h failed
    DYAD_RC_UCXCOMM_FAIL = -3004,     // UCX communication routine failed
    DYAD_RC_UCXMMAP_FAIL = -3005,     // Failed to perform operations with ucp_mem_map
    DYAD_RC_UCXRKEY_PACK_FAILED = -3006,     // Failed to perform operations with ucp_mem_map
    DYAD_RC_UCXRKEY_UNPACK_FAILED = -3007,     // Failed to perform operations with ucp_mem_map

};

typedef enum dyad_core_return_codes dyad_rc_t;

#define DYAD_IS_ERROR(code) ((code) < 0)

#define FLUX_IS_ERROR(code) ((code) < 0)

#ifdef __cplusplus
}
#endif

#endif  // DYAD_DTL_DYAD_RC_H
