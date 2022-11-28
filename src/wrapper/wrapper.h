/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef WRAPPER_H
#define WRAPPER_H

#include "dyad_core.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

extern __thread dyad_ctx_t *ctx;
// extern int open_sync (const char *path);
// extern int close_sync (const char *path);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* WRAPPER_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
