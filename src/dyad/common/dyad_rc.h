#ifndef DYAD_DTL_DYAD_RC_H
#define DYAD_DTL_DYAD_RC_H

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
    DYAD_RC_OK = 0,                 // Operation worked correctly
    DYAD_RC_SYSFAIL = -1,           // Some sys call or C standard
                                    // library call failed
    DYAD_RC_NOCTX = -2,             // No DYAD Context found
    DYAD_RC_FLUXFAIL = -3,          // Some Flux function failed
    DYAD_RC_BADCOMMIT = -4,         // Flux KVS commit didn't work
    DYAD_RC_NOTFOUND = -5,          // Flux KVS lookup didn't work
    DYAD_RC_BADFETCH = -6,          // Flux KVS commit didn't work
    DYAD_RC_BADMETADATA = -7,       // Cannot create/populate a DYAD response
    DYAD_RC_BADRPC = -8,            // Flux RPC pack or get didn't work
    DYAD_RC_BADFIO = -9,            // File I/O failed
    DYAD_RC_BADMANAGEDPATH = -10,   // Cons or Prod Manged Path is bad
    DYAD_RC_BADDTLMODE = -11,       // Invalid DYAD DTL mode provided
    DYAD_RC_BADPACK = -12,          // JSON packing failed
    DYAD_RC_BADUNPACK = -13,        // JSON unpacking failed
    DYAD_RC_UCXINIT_FAIL = -14,     // UCX initialization failed
    DYAD_RC_UCXWAIT_FAIL = -15,     // UCX wait (either custom or
                                    // 'ucp_worker_wait') failed
    DYAD_RC_UCXEP_FAIL = -16,       // An operation on a ucp_ep_h failed
    DYAD_RC_UCXCOMM_FAIL = -17,     // UCX communication routine failed
    DYAD_RC_UCXMMAP_FAIL = -18,     // Failed to perform operations with ucp_mem_map
    DYAD_RC_RPC_FINISHED = -19,     // The Flux RPC responded with ENODATA (i.e.,
                                    // end of stream) sooner than expected
    DYAD_RC_BAD_B64DECODE = -18,    // Decoding of data w/ base64 failed
    DYAD_RC_BAD_COMM_MODE = -19,    // Invalid comm mode provided to DTL
    DYAD_RC_UNTRACKED = -20,        // Provided path is not tracked by DYAD
    DYAD_RC_PERF_INIT_FAIL = -21,   // Performance measurement initialization failed
    DYAD_RC_BAD_CLI_ARG_DEF = -22,  // Trying to define a CLI argument failed
    DYAD_RC_BAD_CLI_PARSE = -23,    // Trying to parse CLI arguments failed
    DYAD_RC_BADBUF = -24,           // Invalid buffer/pointer passed to function
};

typedef enum dyad_core_return_codes dyad_rc_t;

#define DYAD_IS_ERROR(code) ((code) < 0)

#define FLUX_IS_ERROR(code) ((code) < 0)

#ifdef __cplusplus
}
#endif

#endif  // DYAD_DTL_DYAD_RC_H
