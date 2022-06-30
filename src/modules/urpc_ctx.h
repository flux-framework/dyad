/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef URPC_SERVER_CTX_H
#define URPC_SERVER_CTX_H

#include <stdbool.h>
#include <flux/core.h>

struct urpc_server_ctx {
    flux_t *h;
    bool debug;
    flux_msg_handler_t **handlers;
    const char *urpc_cfg_file;
} urpc_server_ctx_default =
    {NULL, false, NULL, NULL};

typedef struct urpc_server_ctx urpc_server_ctx_t;

#endif // URPC_SERVER_CTX_H
