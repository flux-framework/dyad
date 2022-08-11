/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef URPC_CLIENT_H
#define URPC_CLIENT_H

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

uint32_t urpc_get_my_rank (void);
uint32_t urpc_get_service_rank (const char *service_name);
int urpc_client (uint32_t server_rank, const char *cmd, const char *file_path, int is_json);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* URPC_CLIENT_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
