#ifndef DYAD_CORE_DYAD_RC_H
#define DYAD_CORE_DYAD_RC_H

enum dyad_core_return_codes {
    DYAD_RC_OK = 0,               // Operation worked correctly
    DYAD_RC_NOCTX = -1,           // No DYAD Context found
    DYAD_RC_FLUXFAIL = -2,        // Some Flux function failed
    DYAD_RC_BADCOMMIT = -3,       // Flux KVS commit didn't work
    DYAD_RC_BADLOOKUP = -4,       // Flux KVS lookup didn't work
    DYAD_RC_BADFETCH = -5,        // Flux KVS commit didn't work
    DYAD_RC_BADRESPONSE = -6,     // Cannot create/populate a DYAD response
    DYAD_RC_BADRPC = -7,          // Flux RPC pack or get didn't work
    DYAD_RC_BADFIO = -8,          // File I/O failed
    DYAD_RC_BADMANAGEDPATH = -9,  // Cons or Prod Manged Path is bad
};

typedef enum dyad_core_return_codes dyad_rc_t;

#define DYAD_IS_ERROR(code) ((code) < 0)

#endif  // DYAD_CORE_DYAD_RC_H
