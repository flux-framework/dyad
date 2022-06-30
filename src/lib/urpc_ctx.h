/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef URPC_CLIENT_CTX_H
#define URPC_CLIENT_CTX_H

#ifdef __cplusplus
#include <stdbool.h>
#endif // __cplusplus

#include <flux/core.h>


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

struct urpc_client_ctx {
    flux_t *h;
    bool initialized;
    bool debug;
    uint32_t rank; // Flux rank
    char *ns;
} urpc_client_ctx_default =
    {NULL, false, false, 0u, "URPC"};

typedef struct urpc_client_ctx urpc_client_ctx_t;

inline void init_urpc_client_ctx (urpc_client_ctx_t *ctx) {
    ctx->h = NULL;
    ctx->initialized = false;
    ctx->debug = false;
    ctx->rank = 0u;
    ctx->ns = "URPC";
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // URPC_CLIENT_CTX_H
