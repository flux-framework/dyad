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
} dyad_sync_ctx_t_default =
    {NULL, false, false, false, true, false, false, 3u, 1024u, 0u, NULL};

typedef struct dyad_sync_ctx_t dyad_sync_ctx_t;

#endif // DYAD_SYNC_CTX_H
