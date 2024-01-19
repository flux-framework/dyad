/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if defined(__cplusplus)
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif  // defined(__cplusplus)

#include <dyad/common/dyad_rc.h>
#include <dyad/dtl/dyad_dtl_impl.h>
#include <dyad/perf/dyad_perf.h>
#include <dyad/utils/read_all.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

#define TIME_DIFF(Tstart, Tend)                                                                    \
    ((double)(1000000000L * ((Tend).tv_sec - (Tstart).tv_sec) + (Tend).tv_nsec - (Tstart).tv_nsec) \
     / 1000000000L)

struct dyad_mod_ctx {
    flux_t *h;
    bool debug;
    flux_msg_handler_t **handlers;
    const char *dyad_path;
    dyad_dtl_t *dtl_handle;
    dyad_perf_t *perf_handle;
};

const struct dyad_mod_ctx dyad_mod_ctx_default = {NULL, false, NULL, NULL, NULL};

typedef struct dyad_mod_ctx dyad_mod_ctx_t;

static void dyad_mod_fini (void) __attribute__ ((destructor));

void dyad_mod_fini (void)
{
    flux_t *h = flux_open (NULL, 0);

    if (h != NULL) {
    }
}

static void freectx (void *arg)
{
    dyad_mod_ctx_t *ctx = (dyad_mod_ctx_t *)arg;
    flux_msg_handler_delvec (ctx->handlers);
    if (ctx->dtl_handle != NULL) {
        dyad_dtl_finalize (&(ctx->dtl_handle));
        ctx->dtl_handle = NULL;
    }
    if (ctx->perf_handle != NULL) {
        ctx->perf_handle->flush (ctx->perf_handle);
        dyad_perf_finalize (&(ctx->perf_handle));
        ctx->perf_handle = NULL;
    }
    free (ctx);
}

static dyad_mod_ctx_t *getctx (flux_t *h)
{
    dyad_mod_ctx_t *ctx = (dyad_mod_ctx_t *)flux_aux_get (h, "dyad");

    if (!ctx) {
        ctx = (dyad_mod_ctx_t *)malloc (sizeof (*ctx));
        if (ctx == NULL) {
            FLUX_LOG_ERR (h, "DYAD_MOD: could not allocate memory for context");
            goto getctx_error;
        }
        ctx->h = h;
        ctx->debug = false;
        ctx->handlers = NULL;
        ctx->dyad_path = NULL;
        ctx->dtl_handle = NULL;
        if (flux_aux_set (h, "dyad", ctx, freectx) < 0) {
            FLUX_LOG_ERR (h, "DYAD_MOD: flux_aux_set() failed!\n");
            goto getctx_error;
        }
    }

    goto getctx_done;

getctx_error:;
    return NULL;

getctx_done:
    return ctx;
}

/* request callback called when dyad.fetch request is invoked */
#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static void
dyad_fetch_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    // freopen (
    //     "/g/g90/lumsden1/ws/insitu_benchmark/wio_benchmark_dyad_tests/campaigns/"
    //     "pre_christmas_testing/opt_1_oto_scaling/oto_proc_scaling/"
    //     "dyad_1198371a_2n_1pcpn_128i_1si_nocolloc/broker_cb.out",
    //     "a+",
    //     stdout);
    // freopen (
    //     "/g/g90/lumsden1/ws/insitu_benchmark/wio_benchmark_dyad_tests/campaigns/"
    //     "pre_christmas_testing/opt_1_oto_scaling/oto_proc_scaling/"
    //     "dyad_1198371a_2n_1pcpn_128i_1si_nocolloc/broker_cb.err",
    //     "a+",
    //     stderr);
    FLUX_LOG_INFO (h, "Launched callback for dyad.fetch\n");
    dyad_mod_ctx_t *ctx = getctx (h);
    ssize_t inlen = 0;
    void *inbuf = NULL;
    int fd = -1;
    uint32_t userid = 0u;
    char *upath = NULL;
    char fullpath[PATH_MAX + 1] = {'\0'};
    int saved_errno = errno;
    ssize_t file_size = 0;
    dyad_rc_t rc = 0;

    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_server_cb");

    if (!flux_msg_is_streaming (msg)) {
        errno = EPROTO;
        goto fetch_error;
    }

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto fetch_error;

    FLUX_LOG_INFO (h, "DYAD_MOD: unpacking RPC message");

    rc = ctx->dtl_handle->rpc_unpack (ctx->dtl_handle, msg, &upath);

    if (DYAD_IS_ERROR (rc)) {
        FLUX_LOG_ERR (ctx->h, "Could not unpack message from client\n");
        errno = EPROTO;
        goto fetch_error;
    }

    FLUX_LOG_INFO (h, "DYAD_MOD: requested user_path: %s", upath);
    FLUX_LOG_INFO (h, "DYAD_MOD: sending initial response to consumer");

    rc = ctx->dtl_handle->rpc_respond (ctx->dtl_handle, msg);
    if (DYAD_IS_ERROR (rc)) {
        FLUX_LOG_ERR (ctx->h, "Could not send primary RPC response to client\n");
        goto fetch_error;
    }

    strncpy (fullpath, ctx->dyad_path, PATH_MAX - 1);
    concat_str (fullpath, upath, "/", PATH_MAX);

#if DYAD_SPIN_WAIT
    if (!get_stat (fullpath, 1000U, 1000L)) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to access info on \"%s\".\n", fullpath);
        // goto error;
    }
#endif  // DYAD_SPIN_WAIT

    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_server_read_data");
    FLUX_LOG_INFO (h, "Reading file %s for transfer", fullpath);
    fd = open (fullpath, O_RDONLY);
    if (fd < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to open file \"%s\".\n", fullpath);
        DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_read_data");
        goto fetch_error;
    }
    FLUX_LOG_INFO (h, "Getting file size");
    file_size = get_file_size (fd);
    FLUX_LOG_INFO (h, "Getting buffer for reading the file");
    rc = ctx->dtl_handle->get_buffer (ctx->dtl_handle, file_size, &inbuf);
    FLUX_LOG_INFO (h, "Reading file data into buffer");
    if ((inlen = read_all (fd, inbuf, file_size)) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to load file \"%s\".\n", fullpath);
        close (fd);
        DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_read_data");
        goto fetch_error;
    }
    FLUX_LOG_INFO (h, "Closing file pointer");
    close (fd);
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_read_data");
    FLUX_LOG_INFO (h, "Is inbuf NULL? -> %i\n", (int)(inbuf == NULL));

    FLUX_LOG_INFO (h, "Establish DTL connection with consumer");
    rc = ctx->dtl_handle->establish_connection (ctx->dtl_handle);
    if (DYAD_IS_ERROR (rc)) {
        FLUX_LOG_ERR (ctx->h, "Could not establish DTL connection with client\n");
        errno = ECONNREFUSED;
        goto fetch_error;
    }
    FLUX_LOG_INFO (h, "Send file to consumer with DTL");
    rc = ctx->dtl_handle->send (ctx->dtl_handle, inbuf, inlen);
    FLUX_LOG_INFO (h, "Close DTL connection with consumer");
    ctx->dtl_handle->close_connection (ctx->dtl_handle);
    ctx->dtl_handle->return_buffer (ctx->dtl_handle, &inbuf);
    if (DYAD_IS_ERROR (rc)) {
        FLUX_LOG_ERR (ctx->h, "Could not send data to client via DTL\n");
        errno = ECOMM;
        goto fetch_error;
    }

    FLUX_LOG_INFO (h, "Close RPC message stream with an ENODATA (%d) message", ENODATA);
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_server_send_response");
    if (flux_respond_error (h, msg, ENODATA, NULL) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond_error with ENODATA failed\n", __FUNCTION__);
    }
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_send_response");
    errno = saved_errno;
    FLUX_LOG_INFO (h, "Finished dyad.fetch module invocation\n");
    goto end_fetch_cb;

fetch_error:
    FLUX_LOG_ERR (h, "Close RPC message stream with an error (errno = %d)\n", errno);
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_server_send_response");
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond_error", __FUNCTION__);
    }
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_send_response");
    errno = saved_errno;
    return;

end_fetch_cb:
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_server_cb");
    return;
}

static dyad_rc_t dyad_open (flux_t *h, dyad_dtl_mode_t dtl_mode, bool debug, optparse_t *opts)
{
    dyad_mod_ctx_t *ctx = getctx (h);
    dyad_rc_t rc = 0;
    char *e = NULL;

    ctx->debug = debug;
    rc = dyad_perf_init (&(ctx->perf_handle), true, opts);
    if (DYAD_IS_ERROR (rc))
        goto open_done;
    rc = dyad_dtl_init (&(ctx->dtl_handle),
                        dtl_mode,
                        DYAD_COMM_SEND,
                        h,
                        ctx->debug,
                        ctx->perf_handle);

open_done:
    return rc;
}

static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, DYAD_DTL_RPC_NAME, dyad_fetch_request_cb, 0},
     FLUX_MSGHANDLER_TABLE_END};

static struct optparse_option cmdline_opts[] = {{.name = "dtl_mode",
                                                 .key = 'm',
                                                 .has_arg = 1,
                                                 .arginfo = "DTL_MODE",
                                                 .usage = "Specify the data transport "
                                                          "layer mode. Can be one of "
                                                          "'FLUX_RPC' (default) or "
                                                          "'UCX'"},
                                                {.name = "debug",
                                                 .key = 'd',
                                                 .has_arg = 0,
                                                 .usage = "If provided, add debugging "
                                                          "log messages"},
                                                {.name = "info_log",
                                                 .key = 'i',
                                                 .has_arg = 1,
                                                 .arginfo = "INFO_LOG_FILE",
                                                 .usage = "Specify the file into which to redirect "
                                                          "info logging. Does nothing if DYAD was "
                                                          "not configured with "
                                                          "'-DDYAD_LOGGER=PRINTF'"},
                                                {.name = "error_log",
                                                 .key = 'e',
                                                 .has_arg = 1,
                                                 .arginfo = "ERROR_LOG_FILE",
                                                 .usage = "Specify the file into which to redirect "
                                                          "error logging. Does nothing if DYAD was "
                                                          "not configured with "
                                                          "'-DDYAD_LOGGER=PRINTF'"},
                                                OPTPARSE_TABLE_END};

DYAD_DLL_EXPORTED int mod_main (flux_t *h, int argc, char **argv)
{
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    dyad_mod_ctx_t *ctx = NULL;
    size_t arglen = 0;
    bool debug = false;
    dyad_dtl_mode_t dtl_mode = DYAD_DTL_DEFAULT;
    const char *optargp = NULL;
    int optindex = 0;
    optparse_t *opts = NULL;
    int i = 0;

    // TODO(Ian): Remove if not debugging
    // freopen (
    //     "/g/g90/lumsden1/ws/insitu_benchmark/wio_benchmark_dyad_tests/campaigns/"
    //     "pre_christmas_testing/opt_1_oto_scaling/oto_proc_scaling/"
    //     "dyad_1198371a_2n_1pcpn_128i_1si_nocolloc/broker.out",
    //     "a+",
    //     stdout);
    // freopen (
    //     "/g/g90/lumsden1/ws/insitu_benchmark/wio_benchmark_dyad_tests/campaigns/"
    //     "pre_christmas_testing/opt_1_oto_scaling/oto_proc_scaling/"
    //     "dyad_1198371a_2n_1pcpn_128i_1si_nocolloc/broker.err",
    //     "a+",
    //     stderr);

    if (!h) {
        fprintf (stderr, "Failed to get flux handle\n");
        goto mod_done;
    }

    // for (i = 0; i < argc; i++) {
    //     FLUX_LOG_INFO (h, "argv[%d] = %s", i, argv[i]);
    // }

    // FLUX_LOG_INFO (h, "Getting context from AUX");
    ctx = getctx (h);

    // FLUX_LOG_INFO (h, "Creating optparser");
    opts = optparse_create ("dyad.so");
    // FLUX_LOG_INFO (h, "Adding option table to parser");
    if (optparse_add_option_table (opts, cmdline_opts) < 0) {
        // FLUX_LOG_ERR (h, "Cannot add option table for DYAD module");
        fprintf (stderr, "Cannot add option table for DYAD module");
        goto mod_error;
    }
    // FLUX_LOG_INFO (h, "Adding perf options to parser");
    if (DYAD_IS_ERROR (dyad_perf_setopts (opts))) {
        // FLUX_LOG_ERR (h, "Cannot set command-line options for the performance plugin");
        fprintf (stderr, "Cannot set command-line options for the performance plugin");
        goto mod_error;
    }
    // FLUX_LOG_INFO (h, "Parsing command line options");
    if ((optindex = optparse_parse_args (opts, argc, argv)) < 0) {
        // FLUX_LOG_ERR (h, "Cannot parse command line arguments to dyad.so");
        fprintf (stderr, "Cannot parse command line arguments to dyad.so");
        goto mod_error;
    }
    if (optindex >= argc) {
        // FLUX_LOG_ERR (h, "Positional arguments not provided to dyad.so");
        fprintf (stderr, "Positional arguments not provided to dyad.so");
        optparse_print_usage (opts);
        goto mod_error;
    }

    if (optparse_getopt (opts, "info_log", &optargp) > 0) {
        DYAD_LOG_INFO_REDIRECT (optargp);
    } else {
        DYAD_LOG_INFO_REDIRECT ("./dyad_server.out");
    }
    if (optparse_getopt (opts, "error_log", &optargp) > 0) {
        DYAD_LOG_ERR_REDIRECT (optargp);
    } else {
        DYAD_LOG_ERR_REDIRECT ("./dyad_server.err");
    }
    FLUX_LOG_INFO (h, "Test");

    if (optparse_getopt (opts, "dtl_mode", &optargp) > 0) {
        FLUX_LOG_INFO (h, "Found 'dtl_mode': %s", optargp);
        arglen = strlen (optargp);
        if (strncmp (optargp, "FLUX_RPC", arglen) == 0) {
            dtl_mode = DYAD_DTL_FLUX_RPC;
        } else if (strncmp (optargp, "UCX", arglen) == 0) {
            dtl_mode = DYAD_DTL_UCX;
        } else {
            FLUX_LOG_ERR (h, "Invalid DTL mode provided\n");
            optparse_print_usage (opts);
            goto mod_error;
        }
    } else {
        FLUX_LOG_INFO (h, "Did not find 'dtl_mode'");
    }

    if (optparse_getopt (opts, "debug", &optargp) > 0) {
        FLUX_LOG_INFO (h, "Found 'debug'");
        debug = true;
    } else {
        FLUX_LOG_INFO (h, "Did not find 'debug'");
    }

    (ctx->dyad_path) = argv[optindex];
    mkdir_as_needed (ctx->dyad_path, m);

    if (DYAD_IS_ERROR (dyad_open (h, dtl_mode, debug, opts))) {
        FLUX_LOG_ERR (ctx->h, "dyad_open failed");
        goto mod_error;
    }

    optparse_destroy (opts);

    FLUX_LOG_INFO (ctx->h, "dyad module begins using \"%s\"\n", argv[optindex]);

    if (flux_msg_handler_addvec (ctx->h, htab, (void *)h, &ctx->handlers) < 0) {
        FLUX_LOG_ERR (ctx->h, "flux_msg_handler_addvec: %s\n", strerror (errno));
        goto mod_error;
    }

    if (flux_reactor_run (flux_get_reactor (ctx->h), 0) < 0) {
        FLUX_LOG_ERR (ctx->h, "flux_reactor_run: %s", strerror (errno));
        goto mod_error;
    }

    goto mod_done;

mod_error:;
    return EXIT_FAILURE;

mod_done:;
    return EXIT_SUCCESS;
}

DYAD_DLL_EXPORTED MOD_NAME ("dyad");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
