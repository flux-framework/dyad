/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

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
