/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/core/dyad_core.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/utils/read_all.h>
#include <dyad/utils/utils.h>

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

#include <fcntl.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#define TIME_DIFF(Tstart, Tend)                                                                    \
    ((double)(1000000000L * ((Tend).tv_sec - (Tstart).tv_sec) + (Tend).tv_nsec - (Tstart).tv_nsec) \
     / 1000000000L)

struct dyad_mod_ctx {
    flux_msg_handler_t **handlers;
    dyad_ctx_t* ctx;
};

const struct dyad_mod_ctx dyad_mod_ctx_default = {NULL, NULL};

typedef struct dyad_mod_ctx dyad_mod_ctx_t;

static void dyad_mod_fini (void) __attribute__ ((destructor));

void dyad_mod_fini (void)
{
    flux_t *h = flux_open (NULL, 0);

    if (h != NULL) {
    }
#ifdef DYAD_PROFILER_DLIO_PROFILER
    DLIO_PROFILER_C_FINI ();
#endif
}

static void freectx (void *arg)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)arg;
    flux_msg_handler_delvec (mod_ctx->handlers);
    if (mod_ctx->ctx) {
        if (mod_ctx->ctx->dtl_handle) dyad_dtl_finalize (mod_ctx->ctx);
        if (mod_ctx->ctx->prod_managed_path) {
            free (mod_ctx->ctx->prod_managed_path);
            mod_ctx->ctx->prod_managed_path = NULL;
        }
        mod_ctx->ctx->dtl_handle = NULL;
        free (mod_ctx->ctx);
        mod_ctx->ctx = NULL;
    }
    free (mod_ctx);
}

static dyad_mod_ctx_t *getctx (flux_t *h)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)flux_aux_get (h, "dyad");

    if (!mod_ctx) {
        mod_ctx = (dyad_mod_ctx_t *)malloc (sizeof (*mod_ctx));
        if (mod_ctx == NULL) {
            DYAD_LOG_STDERR("DYAD_MOD: could not allocate memory for context");
            goto getctx_error;
        }
        mod_ctx->handlers = NULL;
        mod_ctx->ctx = (dyad_ctx_t *)malloc (sizeof (dyad_ctx_t));
        mod_ctx->ctx->h = h;
        mod_ctx->ctx->debug = false;
        mod_ctx->ctx->prod_managed_path = NULL;
        if (flux_aux_set (h, "dyad", mod_ctx, freectx) < 0) {
            DYAD_LOG_STDERR ("DYAD_MOD: flux_aux_set() failed!");
            goto getctx_error;
        }
    }

    goto getctx_done;

getctx_error:;
    return NULL;

getctx_done:
    return mod_ctx;
}

/* request callback called when dyad.fetch request is invoked */
#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static void
dyad_fetch_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    DYAD_C_FUNCTION_START();
    dyad_mod_ctx_t *mod_ctx = getctx (h);
    DYAD_LOG_INFO(mod_ctx->ctx, "Launched callback for %s", DYAD_DTL_RPC_NAME);
    ssize_t inlen = 0;
    char *inbuf = NULL;
    int fd = -1;
    uint32_t userid = 0u;
    char *upath = NULL;
    char fullpath[PATH_MAX + 1] = {'\0'};
    int saved_errno = errno;
    ssize_t file_size = 0;
    dyad_rc_t rc = 0;
    struct flock shared_lock;
    if (!flux_msg_is_streaming (msg)) {
        errno = EPROTO;
        goto fetch_error_wo_flock;
    }

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto fetch_error_wo_flock;

    DYAD_LOG_INFO(mod_ctx->ctx, "DYAD_MOD: unpacking RPC message");

    rc = mod_ctx->ctx->dtl_handle->rpc_unpack (mod_ctx->ctx, msg, &upath);

    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR(mod_ctx->ctx, "Could not unpack message from client");
        errno = EPROTO;
        goto fetch_error_wo_flock;
    }
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_LOG_DEBUG(mod_ctx->ctx, "DYAD_MOD: requested user_path: %s", upath);
    DYAD_LOG_DEBUG(mod_ctx->ctx, "DYAD_MOD: sending initial response to consumer");

    rc = mod_ctx->ctx->dtl_handle->rpc_respond (mod_ctx->ctx, msg);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR(mod_ctx->ctx, "Could not send primary RPC response to client");
        goto fetch_error_wo_flock;
    }

    strncpy (fullpath, mod_ctx->ctx->prod_managed_path, PATH_MAX - 1);
    concat_str (fullpath, upath, "/", PATH_MAX);
    DYAD_C_FUNCTION_UPDATE_STR ("fullpath", fullpath);

#if DYAD_SPIN_WAIT
    if (!get_stat (fullpath, 1000U, 1000L)) {
        DYAD_LOG_ERR(mod_ctx->ctx, "DYAD_MOD: Failed to access info on \"%s\".", fullpath);
        // goto error;
    }
#endif  // DYAD_SPIN_WAIT

    DYAD_LOG_INFO (mod_ctx->ctx, "Reading file %s for transfer", fullpath);
    fd = open (fullpath, O_RDONLY);

    if (fd < 0) {
        DYAD_LOG_ERROR(mod_ctx->ctx, "DYAD_MOD: Failed to open file \"%s\".", fullpath);
        goto fetch_error_wo_flock;
    }
    rc = dyad_shared_flock (mod_ctx->ctx, fd, &shared_lock);
    if (DYAD_IS_ERROR (rc)) {
        goto fetch_error;
    }
    file_size = get_file_size (fd);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "file %s has size %zd", fullpath, file_size);
    if (file_size > 0) {
#ifdef DYAD_ENABLE_UCX_RMA
        /**
         * To reduce the number of RMA calls, we are encoding file size at the start of the buffer
         */
        size_t fs = file_size;
#endif
        rc = mod_ctx->ctx->dtl_handle->get_buffer (mod_ctx->ctx, file_size, (void**)&inbuf);
#ifdef DYAD_ENABLE_UCX_RMA
        memcpy (inbuf, &fs, sizeof(fs));
        inlen = read (fd, inbuf + sizeof(size_t), file_size);
#else
        inlen = read (fd, inbuf, file_size);
#endif
        if (inlen != file_size) {
            DYAD_LOG_ERROR (mod_ctx->ctx, \
                            "DYAD_MOD: Failed to load file \"%s\" only read %zd of %zd.", \
                            fullpath, \
                            inlen, \
                            file_size);
            goto fetch_error;
        }
#ifdef DYAD_ENABLE_UCX_RMA
        inlen = file_size + sizeof(size_t);
#endif
        DYAD_C_FUNCTION_UPDATE_INT ("file_size", file_size);
        DYAD_LOG_DEBUG (mod_ctx->ctx, "Closing file pointer");
        dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
        close (fd);
        DYAD_LOG_DEBUG (mod_ctx->ctx, "Is inbuf NULL? -> %i", (int)(inbuf == NULL));
        DYAD_LOG_DEBUG (mod_ctx->ctx, "Establish DTL connection with consumer");
        rc = mod_ctx->ctx->dtl_handle->establish_connection (mod_ctx->ctx);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx, "Could not establish DTL connection with client");
            errno = ECONNREFUSED;
            goto fetch_error_wo_flock;
        }
        DYAD_LOG_DEBUG (mod_ctx->ctx, "Send file to consumer with DTL");
        rc = mod_ctx->ctx->dtl_handle->send (mod_ctx->ctx, inbuf, inlen);
        DYAD_LOG_DEBUG (mod_ctx->ctx, "Close DTL connection with consumer");
        mod_ctx->ctx->dtl_handle->close_connection (mod_ctx->ctx);
        mod_ctx->ctx->dtl_handle->return_buffer (mod_ctx->ctx, (void**)&inbuf);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx, "Could not send data to client via DTL\n");
            errno = ECOMM;
            goto fetch_error_wo_flock;
        }
    } else {
        dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
        close (fd);
    }
    DYAD_LOG_DEBUG(mod_ctx->ctx, "Close RPC message stream with an ENODATA (%d) message", ENODATA);
    if (flux_respond_error (h, msg, ENODATA, NULL) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: %s: flux_respond_error with ENODATA failed\n", __func__ );
    }
    DYAD_LOG_INFO(mod_ctx->ctx, "Finished %s module invocation\n", DYAD_DTL_RPC_NAME);
    goto end_fetch_cb;

fetch_error:;
    dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
    close (fd);

fetch_error_wo_flock:;
    DYAD_LOG_ERROR(mod_ctx->ctx, "Close RPC message stream with an error (errno = %d)\n", errno);
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: %s: flux_respond_error", __func__);
    }
    errno = saved_errno;
    DYAD_C_FUNCTION_END();
    return;

end_fetch_cb:;
    errno = saved_errno;
    DYAD_C_FUNCTION_END();
    return;
}

static dyad_rc_t dyad_open (dyad_ctx_t* ctx, dyad_dtl_mode_t dtl_mode)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    rc = dyad_dtl_init (ctx, dtl_mode, DYAD_COMM_SEND, ctx->debug);
    DYAD_C_FUNCTION_END();
    return rc;
}

static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, DYAD_DTL_RPC_NAME, dyad_fetch_request_cb, 0},
     FLUX_MSGHANDLER_TABLE_END};

/** This is a temporary measure until environment variable based initialization
 *  is implemented */
static dyad_dtl_mode_t get_dtl_mode_env ()
{
    char* e = NULL;

    size_t dtl_mode_env_len = 0ul;

    if ((e = getenv (DYAD_DTL_MODE_ENV))) {
        dtl_mode_env_len = strlen (e);
        if (strncmp (e, "FLUX_RPC", dtl_mode_env_len) == 0) {
            return DYAD_DTL_FLUX_RPC;
        } else if (strncmp (e, "UCX", dtl_mode_env_len) == 0) {
            return DYAD_DTL_UCX;
        } else {
            DYAD_LOG_STDERR ("DYAD MOD: Invalid env %s = %s. Defaulting to %s\n", \
                         DYAD_DTL_MODE_ENV, e, dyad_dtl_mode_name[DYAD_DTL_DEFAULT]);
            return DYAD_DTL_DEFAULT;
        }
    } else {
        DYAD_LOG_STDERR ("DYAD MOD: %s is not set. Defaulting to %s\n", \
                         DYAD_DTL_MODE_ENV, dyad_dtl_mode_name[DYAD_DTL_DEFAULT]);
        return DYAD_DTL_DEFAULT;
    }
    return DYAD_DTL_DEFAULT;
}

static void show_help (void)
{
    DYAD_LOG_STDOUT ("dyad module options and arguments\n");
    DYAD_LOG_STDOUT ("    -h, --help:  Show help.\n");
    DYAD_LOG_STDOUT ("    -d, --debug: Enable debugging log message.\n");
    DYAD_LOG_STDOUT ("    -m, --mode:  DTL mode. Need an argument.\n"
                     "                 Either 'FLUX_RPC' (default) or 'UCX'.\n");
    DYAD_LOG_STDOUT ("    -i, --info_log: Specify the file into which to redirect\n"
                     "                    info logging. Does nothing if DYAD was not\n"
                     "                    configured with '-DDYAD_LOGGER=PRINTF'.\n"
                     "                    Need a filename as an argument.\n");
    DYAD_LOG_STDOUT ("    -e, --error_log: Specify the file into which to redirect\n"
                     "                     error logging. Does nothing if DYAD was\n"
                     "                     not configured with '-DDYAD_LOGGER=PRINTF'\n"
                     "                     Need a filename as an argument.\n");
}

static int opt_parse (dyad_ctx_t* ctx, const unsigned broker_rank,
                      dyad_dtl_mode_t *dtl_mode, int _argc, char** _argv)
{
    size_t arglen = 0ul;
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
  #ifndef DYAD_LOGGER_NO_LOG
    char log_file_name[PATH_MAX+1] = {'\0'};
    char err_file_name[PATH_MAX+1] = {'\0'};
    sprintf (log_file_name, "dyad_mod_%u.out", broker_rank);
    sprintf (err_file_name, "dyad_mod_%d.err", broker_rank);
  #endif // DYAD_LOGGER_NO_LOG
    *dtl_mode = DYAD_DTL_END;

    int argc = 0;
    char* argv[PATH_MAX] = {NULL};

    if ((argc = _argc + 1) > PATH_MAX) {
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: too many options.\n");
        return DYAD_RC_SYSFAIL ;
    }

    for (int i = 1; i < argc; ++i) {
        argv[i] = _argv[i-1];
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: argv[%d] = '%s'\n", i, argv[i]);
    }

    while (1)
    {
        static struct option long_options[] =
        {
            {"help",      no_argument,       0, 'h'},
            {"debug",     no_argument,       0, 'd'},
            {"mode",      required_argument, 0, 'm'},
            {"info_log",  required_argument, 0, 'i'},
            {"error_log", required_argument, 0, 'e'},
            {0, 0, 0, 0}
	    };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        int c = -1;

        c = getopt_long (argc, argv, "hdm:i:e:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) {
            DYAD_LOG_DEBUG (ctx, "DYAD_MOD: no more option.\n");
            break;
        }
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: opt %c, index %d\n", (char) c, optind);

        switch (c)
        {
            case 'h':
                show_help ();
                break;
            case 'd':
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'debug' option -d \n");
                ctx->debug = true;
                break;
            case 'm':
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: DTL 'mode' option -m with value `%s'\n", optarg);
                arglen = strlen (optarg);
                if (strncmp (optarg, "FLUX_RPC", arglen) == 0) {
                    *dtl_mode = DYAD_DTL_FLUX_RPC;
                } else if (strncmp (optarg, "UCX", arglen) == 0) {
                    *dtl_mode = DYAD_DTL_UCX;
                } else {
                    DYAD_LOG_ERROR (ctx, "DYAD_MOD: Invalid DTL 'mode' (%s) provided\n", optarg);
                    *dtl_mode = DYAD_DTL_END;
                    show_help ();
                    return DYAD_RC_SYSFAIL ;
                }
                break;
            case 'i':
              #ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'info_log' option -i with value `%s'\n", optarg);
                sprintf (log_file_name, "%s_%u.out", optarg, broker_rank);
              #endif // DYAD_LOGGER_NO_LOG
                break;
            case 'e':
              #ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'error_log' option -e with value `%s'\n", optarg);
                sprintf (err_file_name, "%s_%d.err", optarg, broker_rank);
              #endif // DYAD_LOGGER_NO_LOG
                break;
            case '?':
                /* getopt_long already printed an error message. */
                break;
            default:
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: option parsing failed %d\n", c);
                return DYAD_RC_SYSFAIL ;
        }
    }

  #ifndef DYAD_LOGGER_NO_LOG
    DYAD_LOG_STDOUT_REDIRECT (log_file_name);
    DYAD_LOG_STDERR_REDIRECT (err_file_name);
  #endif // DYAD_LOGGER_NO_LOG

    if (*dtl_mode == DYAD_DTL_END) {
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: Did not find DTL 'mode' option");
        *dtl_mode = get_dtl_mode_env ();
    }

    /* Print any remaining command line arguments (not options). */
    while (optind < argc) {
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: positional arguments %s\n", argv[optind]);
        ctx->prod_managed_path = argv[optind++];
    }
    if (ctx->prod_managed_path) {
        const size_t prod_path_len = strlen (ctx->prod_managed_path);
        const char* tmp_ptr = ctx->prod_managed_path;
        ctx->prod_managed_path = (char*)malloc (prod_path_len + 1);
        if (ctx->prod_managed_path == NULL) {
            DYAD_LOG_ERROR (ctx, "DYAD_MOD: Could not allocate buffer for " \
                                 "Producer managed path!\n");
            return DYAD_RC_SYSFAIL ;
        }
        strncpy (ctx->prod_managed_path, tmp_ptr, prod_path_len + 1);
        (ctx->prod_managed_path)[prod_path_len] = '\0';
        mkdir_as_needed (ctx->prod_managed_path, m);
        DYAD_LOG_INFO (ctx, "DYAD_MOD: Loading DYAD Module with Path %s and DTL Mode %s", \
                       ctx->prod_managed_path, dyad_dtl_mode_name[*dtl_mode]);
    }
    return DYAD_RC_OK;
}

DYAD_DLL_EXPORTED int mod_main (flux_t *h, int argc, char **argv)
{
    DYAD_LOGGER_INIT ();
    DYAD_LOG_STDOUT ("Loading mod_main\n");
    dyad_mod_ctx_t *mod_ctx = NULL;
    dyad_dtl_mode_t dtl_mode = DYAD_DTL_DEFAULT;

    if (!h) {
        DYAD_LOG_STDERR ("Failed to get flux handle\n");
        goto mod_done;
    }

    mod_ctx = getctx (h);

    uint32_t broker_rank;
    flux_get_rank (h, &broker_rank);
#ifdef DYAD_PROFILER_DLIO_PROFILER
    int pid = broker_rank;
    DLIO_PROFILER_C_INIT (NULL, NULL, &pid);
#endif
    DYAD_C_FUNCTION_START ();

    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Parsing command line options");
    if (opt_parse (mod_ctx->ctx, broker_rank, &dtl_mode, argc, argv) != DYAD_RC_OK) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Cannot parse command line arguments");
        goto mod_error;
    }


    if (DYAD_IS_ERROR (dyad_open (mod_ctx->ctx, dtl_mode))) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: dyad_open failed");
        goto mod_error;
    }

    if (flux_msg_handler_addvec (mod_ctx->ctx->h, htab, (void *)h, &mod_ctx->handlers) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: flux_msg_handler_addvec: %s\n", strerror (errno));
        goto mod_error;
    }

    if (flux_reactor_run (flux_get_reactor (mod_ctx->ctx->h), 0) < 0) {
        DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: flux_reactor_run: %s", strerror (errno));
        goto mod_error;
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Finished");
    goto mod_done;

mod_error:;
    DYAD_C_FUNCTION_END ();
    return EXIT_FAILURE;

mod_done:;
    DYAD_C_FUNCTION_END ();
    return EXIT_SUCCESS;
}

DYAD_DLL_EXPORTED MOD_NAME ("dyad");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
