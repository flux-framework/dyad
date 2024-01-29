#ifndef DYAD_COMMON_STRUCTURES_H
#define DYAD_COMMON_STRUCTURES_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif
#include <flux/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct dyad_ctx
 */
struct dyad_ctx {
    // Internal
    flux_t* h;                      // the Flux handle for DYAD
    struct dyad_dtl* dtl_handle;    // Opaque handle to DTL info
    const char* fname;              // Used to track which file is getting processed.
    bool use_fs_locks;              // Used to track if fs locks should be used.
    // User Facing
    bool debug;                     // if true, perform debug logging
    bool check;                     // if true, perform some check logging
    bool reenter;                   // if false, do not recursively enter DYAD
    bool initialized;               // if true, DYAD is initialized
    bool shared_storage;            // if true, the managed path is shared
    unsigned int key_depth;         // Depth of bins for the Flux KVS
    unsigned int key_bins;          // Number of bins for the Flux KVS
    uint32_t rank;                  // Flux rank for DYAD
    uint32_t service_mux;           // Number of Flux brokers sharing node-local storage
    uint32_t node_idx;              // Index of the node hosting broker(s)
    int pid;                        // unix process id, obtained by getpid()
    char* kvs_namespace;            // Flux KVS namespace for DYAD
    char* prod_managed_path;        // producer path managed by DYAD
    char* cons_managed_path;        // consumer path managed by DYAD
};
typedef struct dyad_ctx dyad_ctx_t;
typedef void* ucx_ep_cache_h;


#ifdef __cplusplus
}
#endif

#endif  // DYAD_COMMON_STRUCTURES_H
