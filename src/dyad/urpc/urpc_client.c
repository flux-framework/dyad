/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

#if defined(__cplusplus)
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
using namespace std;  // std::clock ()
// #include <cstdbool> // c++11
#else
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif  // defined(__cplusplus)

#include <dlfcn.h>
#include <fcntl.h>
#include <flux/core.h>
#include <libgen.h>  // dirname
#include <unistd.h>

#include "urpc_ctx.h"
#include "utils.h"

// Debug message
#ifndef DPRINTF
#define DPRINTF(fmt, ...)                         \
    do {                                          \
        if (ctx && ctx->debug)                    \
            fprintf (stderr, fmt, ##__VA_ARGS__); \
    } while (0)
#endif  // DPRINTF

// Detailed information message that can be omitted
#if URPC_FULL_DEBUG
#define IPRINTF DPRINTF
#define IPRINTF_DEFINED
#else
#define IPRINTF(fmt, ...)
#endif  // URPC_FULL_DEBUG

#if !defined(URPC_LOGGING_ON) || (URPC_LOGGING_ON == 0)
#define FLUX_LOG_INFO(...) \
    do {                   \
    } while (0)
#define FLUX_LOG_ERR(...) \
    do {                  \
    } while (0)
#else
#define FLUX_LOG_INFO(...) flux_log (ctx->h, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(...) flux_log_error (ctx->h, __VA_ARGS__)
#endif

__thread urpc_client_ctx_t *ctx = NULL;
static void urpc_client_init (void) __attribute__ ((constructor));
static void urpc_client_fini (void) __attribute__ ((destructor));

uint32_t urpc_get_my_rank (void)
{
#if 0
    if ((ctx == NULL) || (ctx->h == NULL) ||
        (flux_get_rank (ctx->h, &ctx->rank) < 0) ){
        FLUX_LOG_ERR ("flux_get_rank() failed.\n");
        return 0u;
    }
#endif

    return ctx->rank;
}

uint32_t urpc_get_service_rank (const char *service_name)
{
    uint32_t service_rank = 0u;
    /*
        flux_future_t *f1 = NULL;

        f1 = flux_kvs_lookup (ctx->h, ctx->ns, FLUX_KVS_WAITCREATE,
       service_name); if (f1 == NULL) { FLUX_LOG_ERR ("flux_kvs_lookup(%s)
       failed.\n", topic); goto done;
        }
    */
    return service_rank;
}

#if URPC_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif  // URPC_PERFFLOW
static int
urpc_rpc_pack (flux_future_t **ft, uint32_t rank, const char *cmd)
{
    // Send the request to execute a command
    if (!(*ft = flux_rpc_pack (ctx->h, "urpc.exec", rank, 0, "{s:s}", "cmd", cmd))) {
        FLUX_LOG_ERR ("flux_rpc_pack({urpc.exec %s})", cmd);
        return -1;
    }

    return 0;
}

#if URPC_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif  // URPC_PERFFLOW
static int
urpc_rpc_get_raw (flux_future_t *ft, const char **result, int *result_size)
{
    // Extract the data from reply
    if (flux_rpc_get_raw (ft, (const void **)result, result_size) < 0) {
        FLUX_LOG_ERR ("flux_rpc_get_raw() failed.\n");
        return -1;
    }
    return 0;
}

// TODO
// function to obtain the server rank from KVS by service name

/**
 *  Write the result of remotely run command `cmd` into a temporary file
 *  located under `file_path`.
 */
#if URPC_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int urpc_client (uint32_t server_rank, const char *cmd, const char *file_path, int is_json)
{
    int rc = -1;
    flux_future_t *ft = NULL;
    const char *result = NULL;
    int result_size = 0;
    const char *odir = NULL;
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    FILE *of = NULL;

    char file_path_copy[PATH_MAX + 1] = {'\0'};

    if (!ctx->h)
        goto done;

    FLUX_LOG_INFO ("URPC_CLIENT: urpc_client() for \"%s\".\n", cmd);

    if (is_json) {
        if (!(ft = flux_rpc (ctx->h, "urpc.execj", cmd, server_rank, 0))) {
            FLUX_LOG_ERR ("flux_rpc({urpc.execj %s})", cmd);
            goto done;
        }
    } else {
        if (urpc_rpc_pack (&ft, server_rank, cmd))
            goto done;
    }
    if (urpc_rpc_get_raw (ft, &result, &result_size))
        goto done;

    FLUX_LOG_INFO ("The size of result received: %d.\n", result_size);

    if ((file_path != NULL) && (strlen (file_path) > 0u)) {
        // Set output file path
        strncpy (file_path_copy, file_path,
                 PATH_MAX);  // dirname modifies the arg

        // Create the directory as needed
        // TODO: Need to be consistent with the mode at the source
        odir = dirname (file_path_copy);
        m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
        if ((strncmp (odir, ".", strlen (".")) != 0) && (mkdir_as_needed (odir, m) < 0)) {
            FLUX_LOG_ERR ("Failed to create directory \"%s\".\n", odir);
            goto done;
        }

        // Write the file.
        of = fopen (file_path, "w");
        if (of == NULL) {
            FLUX_LOG_ERR ("Could not open to write file: %s\n", file_path);
            goto done;
        }
        if (fwrite (result, sizeof (char), (size_t)result_size, of)
            != (size_t)result_size) {
            FLUX_LOG_ERR ("Could not write file: %s\n", file_path);
            goto done;
        }
        // Close the file
        rc = ((fclose (of) == 0) ? 0 : -1);
    } else {
        rc = 0;
        if (result_size > 0) {
            printf ("%.*s", result_size, result);
        }
    }

    flux_future_destroy (ft);
    ft = NULL;

done:
    if (ft != NULL) {
        flux_future_destroy (ft);
    }
    // TODO: return the return code of cmd
    return rc;
}

/*****************************************************************************
 *                                                                           *
 *         URPC Client Constructor, Destructor and Wrapper API                 *
 *                                                                           *
 *****************************************************************************/

void urpc_client_init (void)
{
    char *e = NULL;

    if (ctx != NULL) {
        if (ctx->initialized) {
            IPRINTF ("URPC_CLIENT: Already initialized.\n");
        } else {
            *ctx = urpc_client_ctx_default;
            // init_urpc_client_ctx (ctx);
        }
        return;
    }

    if (!(ctx = (urpc_client_ctx_t *)malloc (sizeof (urpc_client_ctx_t))))
        exit (1);

    ctx->initialized = true;

    DPRINTF ("URPC_CLIENT: Initializeing URPC client\n");

    if (!(ctx = (urpc_client_ctx_t *)malloc (sizeof (urpc_client_ctx_t))))
        exit (1);

    *ctx = urpc_client_ctx_default;

    if ((e = getenv ("URPC_CLIENT_DEBUG"))) {
        ctx->debug = true;
    } else {
        ctx->debug = false;
    }

    if (!(ctx->h = flux_open (NULL, 0))) {
        DPRINTF ("URPC_CLIENT: can't open flux/\n");
    }

    if (flux_get_rank (ctx->h, &ctx->rank) < 0) {
        FLUX_LOG_ERR ("flux_get_rank() failed.\n");
    }

    if ((e = getenv ("URPC_NAMESPACE")))
        ctx->ns = e;
    else
        ctx->ns = "URPC";

    FLUX_LOG_INFO ("URPC Initialized\n");
    FLUX_LOG_INFO ("URPC_CLIENT_DEBUG=%s\n", (ctx->debug) ? "true" : "false");
}

void urpc_client_fini ()
{
    free (ctx);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
