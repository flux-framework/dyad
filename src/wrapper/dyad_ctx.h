/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_SYNC_CTX_H
#define DYAD_SYNC_CTX_H

#if !defined(__cplusplus)
#include <stdbool.h>
#endif // !defined(__cplusplus)

#include <flux/core.h>
#include "dyad_cpa.h"

struct dyad_sync_ctx_t {
    flux_t *h;
    bool debug;
    bool check;
    bool reenter;
    bool initialized;
    // Indicate if the storage associated with the managed path is shared
    // (i.e. visible to all ranks)
    bool shared_storage;
    bool sync_started; // barrier at the beginning for synchronized start
    unsigned int key_depth; // The depth of the key hierarchy for path
    unsigned int key_bins; // The number of bins used in key hashing
    uint32_t rank; // Flux rank
    const char* kvs_namespace; // The KVS namespace of the sharing context
  #if DYAD_PROFILE
    double t_kvs_prod; // Total time to publish the ownership of files via KVS
    double t_kvs_cons; // Total time to find the ownership of files via KVS
    double t_rpc_cons; // Total time to retrieve files from publishers
    double t_sync_io_cons;  // Local file IO cost used for sync only
    double t_sync_prod;  // Total sync cost for producer
    double t_sync_cons;  // Total sync cost for consumer including idle wait
    double t_hash_prod;  // file name hashing cost by producer, included in sync
    double t_hash_cons;  // file name hashing cost by consumer, included in sync
    const char* profile_tag; // To show the result with a distinguishing tag
  #endif // DYAD_PROFILE
  #if DYAD_CPA
    dyad_cpa_t *t_rec;
    unsigned int t_rec_capacity;
    unsigned int t_rec_size;
   #if DYAD_CPA_UNIQUE_PRODUCER
    uint32_t prod_rank;
   #endif // DYAD_CPA_UNIQUE_PRODUCER
    FILE *cpa_outfile;
  #endif // DYAD_CPA
} dyad_sync_ctx_t_default =
  #if DYAD_PROFILE
    {NULL, false, false, false, true, false, false, 3u, 1024u, 0u, NULL,
     0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, NULL};
  #else
    {NULL, false, false, false, true, false, false, 3u, 1024u, 0u, NULL};
  #endif // DYAD_PROFILE

typedef struct dyad_sync_ctx_t dyad_sync_ctx_t;

#endif // DYAD_SYNC_CTX_H
