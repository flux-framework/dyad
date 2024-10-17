#ifndef DYAD_COMMON_STRUCTURES_INT_H
#define DYAD_COMMON_STRUCTURES_INT_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_structures.h>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct dyad_ctx
 */
struct dyad_ctx {
  // Internal
  void *h;                     // the Flux handle for DYAD
  struct dyad_dtl *dtl_handle; // Opaque handle to DTL info
  const char *fname;           // Used to track which file is getting processed.
  bool use_fs_locks;           // Used to track if fs locks should be used.
  char *prod_real_path;        // producer managed real path
  char *cons_real_path;        // consumer managed real path
  uint32_t prod_managed_len;   // length of producer path managed by DYAD
  uint32_t cons_managed_len;   // length of consumer path managed by DYAD
  uint32_t prod_real_len;      // length of producer managed real path
  uint32_t cons_real_len;      // length of consumer managed real path
  uint32_t prod_managed_hash;  // hash of producer path managed by DYAD
  uint32_t cons_managed_hash;  // hash of consumer path managed by DYAD
  uint32_t prod_real_hash;     // hash of producer managed real path
  uint32_t cons_real_hash;     // hash of consumer managed real path
  uint32_t delim_len;          // length of path delimiter
  // User Facing
  bool debug;              // if true, perform debug logging
  bool check;              // if true, perform some check logging
  bool reenter;            // if false, do not recursively enter DYAD
  bool initialized;        // if true, DYAD is initialized
  bool shared_storage;     // if true, the managed path is shared
  bool async_publish;      // Enable asynchronous publish by producer
  bool fsync_write;        // Apply fsync after write by producer
  unsigned int key_depth;  // Depth of bins for the Flux KVS
  unsigned int key_bins;   // Number of bins for the Flux KVS
  uint32_t rank;           // Flux rank for DYAD
  uint32_t service_mux;    // Number of Flux brokers sharing node-local storage
  uint32_t node_idx;       // Index of the node hosting broker(s)
  int pid;                 // unix process id, obtained by getpid()
  char *kvs_namespace;     // Flux KVS namespace for DYAD
  char *prod_managed_path; // producer path managed by DYAD
  char *cons_managed_path; // consumer path managed by DYAD
  bool
      relative_to_managed_path; // relative path is relative to the managed path
};
typedef void *ucx_ep_cache_h;

#ifdef __cplusplus
}
#endif

#endif // DYAD_COMMON_STRUCTURES_INT_H
