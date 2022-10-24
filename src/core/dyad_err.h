#ifndef __DYAD_CORE_DYAD_ERR_H__
#define __DYAD_CORE_DYAD_ERR_H__

enum dyad_core_err_codes {
    DYAD_OK = 0, // Operation worked correctly
    DYAD_NOCTX = -1, // No DYAD Context found
    DYAD_FLUXFAIL = -2, // Some Flux function failed
    DYAD_BADCOMMIT = -3, // Flux KVS commit didn't work
    DYAD_BADLOOKUP = -4, // Flux KVS lookup didn't work
    DYAD_BADFETCH = -5, // Flux KVS commit didn't work
    DYAD_BADRESPONSE = -6, // Cannot create/populate a DYAD response
    DYAD_BADRPC = -7, // Flux RPC pack or get didn't work
    DYAD_BADFIO = -8, // File I/O failed
    DYAD_BADMANAGEDPATH = -9, // Cons or Prod Manged Path is bad
};

typedef enum dyad_core_err_codes dyad_core_err_t;

#define DYAD_IS_ERROR(code) (code < 0)

#endif




                                           
                                           
                                           
                                           
                                           
                                           
                                           
                                           
                                           
