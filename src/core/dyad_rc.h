#ifndef DYAD_CORE_DYAD_RC_H
#define DYAD_CORE_DYAD_RC_H

enum dyad_core_return_codes {
    DYAD_RC_OK = 0,               // Operation worked correctly
    DYAD_RC_SYSFAIL = -1,         // Some sys call or C standard
                                  // library call failed
    DYAD_RC_NOCTX = -2,           // No DYAD Context found
    DYAD_RC_FLUXFAIL = -3,        // Some Flux function failed
    DYAD_RC_BADCOMMIT = -4,       // Flux KVS commit didn't work
    DYAD_RC_BADLOOKUP = -5,       // Flux KVS lookup didn't work
    DYAD_RC_BADFETCH = -6,        // Flux KVS commit didn't work
    DYAD_RC_BADRESPONSE = -7,     // Cannot create/populate a DYAD response
    DYAD_RC_BADRPC = -8,          // Flux RPC pack or get didn't work
    DYAD_RC_BADFIO = -9,          // File I/O failed
    DYAD_RC_BADMANAGEDPATH = -10, // Cons or Prod Manged Path is bad
    DYAD_RC_BADDTLMODE = -11,     // Invalid DYAD DTL mode provided
    DYAD_RC_BADPACK = -12,        // JSON RPC payload packing failed
    DYAD_RC_UCXINIT_FAIL = -13,   // UCX initialization failed
    DYAD_RC_UCXWAIT_FAIL = -14,   // UCX wait (either custom or
                                  // 'ucp_worker_wait') failed
    DYAD_RC_UCXCOMM_FAIL = -15,   // UCX communication routine failed
    DYAD_RC_RPC_FINISHED = -16,   // The Flux RPC responded with ENODATA (i.e.,
                                  // end of stream) sooner than expected
};

typedef enum dyad_core_return_codes dyad_rc_t;

#define DYAD_IS_ERROR(code) ((code) < 0)

#endif  // DYAD_CORE_DYAD_RC_H
