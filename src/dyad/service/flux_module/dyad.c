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

// clang-format off
// #include <dyad/core/dyad_core_int.h>
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures_int.h>
#include <dyad/core/dyad_ctx.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/utils/read_all.h>
#include <dyad/utils/utils.h>
// clang-format on

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
#include <getopt.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

#define TIME_DIFF(Tstart, Tend)                                                                    \
    ((double)(1000000000L * ((Tend).tv_sec - (Tstart).tv_sec) + (Tend).tv_nsec - (Tstart).tv_nsec) \
     / 1000000000L)

/**
 * @file dyad.c
 * @brief DYAD Flux broker module implementation.
 *
 * @details
 * Implements the DYAD service as a Flux broker module. Flux services are
 * implemented as dynamically loaded broker plugins ("broker modules") that
 * register message handlers for their methods and run the Flux reactor to
 * remain responsive while handling requests from multiple clients concurrently
 * using event-driven (reactive) programming techniques.
 *
 * This module can be loaded using:
 * @code
 *   flux module load dyad.so [options] [producer_path]
 * @endcode
 *
 * Available options:
 *  - @c -h, @c --help        Show help and exit without loading the module.
 *  - @c -d, @c --debug       Enable debug logging.
 *  - @c -m, @c --mode        DTL mode (@c FLUX_RPC or @c UCX).
 *  - @c -i, @c --info_log    Redirect info logging to a file (requires
 *                            @c -DDYAD_LOGGER=PRINTF at build time).
 *  - @c -e, @c --error_log   Redirect error logging to a file (requires
 *                            @c -DDYAD_LOGGER=PRINTF at build time).
 */

/**
 * @brief Context structure for the DYAD Flux module.
 *
 * @details
 * Holds the Flux message handler table and the DYAD context for a running
 * module instance. Allocated per broker handle via @c get_mod_ctx() and
 * freed at module finalization time via @c freectx().
 */
typedef struct dyad_mod_ctx {
    flux_msg_handler_t **handlers;  ///< Flux message handler table.
    dyad_ctx_t *ctx;                ///< DYAD context for this module instance.
} dyad_mod_ctx_t;

const struct dyad_mod_ctx dyad_mod_ctx_default = {NULL, NULL};

static void dyad_mod_fini (void) __attribute__ ((destructor));

/**
 * @brief Finalizes the DYAD Flux module at library unload time.
 *
 * @details
 * Registered as a destructor via @c __attribute__((destructor)). Called
 * automatically when the module shared library is unloaded by the broker.
 *
 * @note If @c DYAD_PROFILER_DFTRACER is defined, finalizes the DFTracer
 *       profiler. Flux handle operations are intentionally omitted here
 *       as calling @c flux_open() at finalization time is known to cause
 *       errors.
 */
void dyad_mod_fini (void)
{
    // Chen: commented out the following, which
    // seems to be erroneous
    // flux_open() at finalization time often
    // cause errors
    /*
    flux_t *h = flux_open (NULL, 0);
    if (h != NULL) {
    }
    */
#ifdef DYAD_PROFILER_DFTRACER
    DFTRACER_C_FINI ();
#endif
}

/**
 * @brief Frees the DYAD module context at Flux module finalization time.
 *
 * @details
 * Registered as the destructor callback for the @c "dyad" auxiliary data
 * on the Flux handle via @c flux_aux_set(). Called by the Flux broker when
 * the module is unloaded. Releases the message handler table, finalizes
 * the DYAD context via @c dyad_ctx_fini(), and frees the context struct.
 *
 * @param[in] arg  Pointer to the @c dyad_mod_ctx_t to free. Cast from
 *                 @c void* as required by the @c flux_free_f signature.
 */
static void freectx (void *arg)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)arg;
    if (mod_ctx->handlers != NULL) {
        flux_msg_handler_delvec (mod_ctx->handlers);
        mod_ctx->handlers = NULL;
    }
    if (mod_ctx->ctx) {
        dyad_ctx_fini ();
        mod_ctx->ctx = NULL;
    }
    free (mod_ctx);
}

/**
 * @brief Retrieves or allocates the DYAD module context for a Flux handle.
 *
 * @details
 * Looks up the @c dyad_mod_ctx_t associated with @p h via
 * @c flux_aux_get(). If none exists, allocates a new one, initializes it
 * to @c NULL, and registers it with @c flux_aux_set() so that @c freectx()
 * is called automatically at module finalization time.
 *
 * @param[in] h  Flux handle to look up or register the context on.
 *
 * @return Pointer to the @c dyad_mod_ctx_t, or @c NULL if allocation or
 *         registration failed.
 */
static dyad_mod_ctx_t *get_mod_ctx (flux_t *h)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)flux_aux_get (h, "dyad");

    if (!mod_ctx) {
        mod_ctx = (dyad_mod_ctx_t *)malloc (sizeof (*mod_ctx));
        if (mod_ctx == NULL) {
            DYAD_LOG_STDERR ("DYAD_MOD: could not allocate memory for context");
            goto getctx_error;
        }
        mod_ctx->handlers = NULL;
        mod_ctx->ctx = NULL;

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

/**
 * @brief Flux message handler callback that serves file data to a consumer
 *        via RPC.
 *
 * @details
 * Registered as the handler for @c DYAD_DTL_RPC_NAME requests in @c htab.
 * Invoked by the Flux reactor when a consumer dispatches an RPC to the
 * producer's broker requesting file data. Performs the following steps:
 *
 *  1. Validates that the incoming message is a streaming RPC.
 *  2. Unpacks the relative file path (@c upath) from the RPC payload via
 *     @c dtl_handle->rpc_unpack().
 *  3. Sends an initial RPC response to acknowledge the request via
 *     @c dtl_handle->rpc_respond().
 *  4. Resolves the full file path by combining @c prod_managed_path and
 *     @c upath.
 *  5. Opens the file and acquires a shared lock via @c dyad_shared_flock()
 *     to allow concurrent reads while blocking exclusive (producer) locks.
 *  6. Reads the file contents into a DTL buffer. For large files (at or
 *     above @c DYAD_POSIX_TRANSFER_GRANULARITY bytes), reads in chunks.
 *  7. Releases the shared lock, establishes a DTL connection to the
 *     consumer via @c dtl_handle->establish_connection(), and sends the
 *     data via @c dtl_handle->send().
 *  8. Closes the DTL connection and signals end-of-stream to the consumer
 *     by responding with @c ENODATA via @c flux_respond_error().
 *
 * On any error, responds to the consumer with the current @c errno via
 * @c flux_respond_error() and returns. The shared lock and file descriptor
 * are released before returning in all error paths.
 *
 * When built with UCX DTL support (@c DYAD_ENABLE_UCX_DTL), the file size
 * is prepended to the DTL buffer so the consumer can locate the data
 * boundary without an additional RMA call.
 *
 * When built with @c DYAD_SPIN_WAIT, spins on @c get_stat() before
 * opening the file to wait for it to become accessible.
 *
 * @param[in] h    Flux handle for the broker.
 * @param[in] w    Flux message handler (unused directly).
 * @param[in] msg  Incoming Flux RPC message containing the file path
 *                 packed by the consumer.
 * @param[in] arg  Auxiliary argument (the Flux handle, passed as @c void*
 *                 from @c flux_msg_handler_addvec()).
 *
 * @note This function is an internal Flux message handler and is not
 *       intended to be called directly. It is registered via @c htab in
 *       @c mod_main().
 * @note The shared lock acquired in step 5 coordinates with the exclusive
 *       lock held by the producer during a write. Because POSIX @c fcntl
 *       locks are cooperative, this only provides guarantees between
 *       processes that also participate in locking.
 */
#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static void
dyad_fetch_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    DYAD_C_FUNCTION_START ();
    dyad_mod_ctx_t *mod_ctx = get_mod_ctx (h);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Launched callback for %s", DYAD_DTL_RPC_NAME);
    ssize_t inlen = 0l;
    char *inbuf = NULL;
    int fd = -1;
    uint32_t userid = 0u;
    char *upath = NULL;
    char fullpath[PATH_MAX + 1] = {'\0'};
    int saved_errno = errno;
    ssize_t file_size = 0l;
    dyad_rc_t rc = 0;
    struct flock shared_lock;
    if (!flux_msg_is_streaming (msg)) {
        errno = EPROTO;
        goto fetch_error_wo_flock;
    }

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto fetch_error_wo_flock;

    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: unpacking RPC message");

    rc = mod_ctx->ctx->dtl_handle->rpc_unpack (mod_ctx->ctx, msg, &upath);

    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not unpack message from client");
        errno = EPROTO;
        goto fetch_error_wo_flock;
    }
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: requested user_path: %s", upath);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: sending initial response to consumer");

    rc = mod_ctx->ctx->dtl_handle->rpc_respond (mod_ctx->ctx, msg);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not send primary RPC response to client");
        goto fetch_error_wo_flock;
    }

    strncpy (fullpath, mod_ctx->ctx->prod_managed_path, PATH_MAX - 1);
    concat_str (fullpath, upath, "/", PATH_MAX);
    DYAD_C_FUNCTION_UPDATE_STR ("fullpath", fullpath);

#if DYAD_SPIN_WAIT
    if (!get_stat (fullpath, 1000U, 1000L)) {
        DYAD_LOG_ERR (mod_ctx->ctx, "DYAD_MOD: Failed to access info on \"%s\".", fullpath);
        // goto error;
    }
#endif  // DYAD_SPIN_WAIT

    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Reading file %s for transfer", fullpath);
    fd = open (fullpath, O_RDONLY);

    if (fd < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Failed to open file \"%s\".", fullpath);
        goto fetch_error_wo_flock;
    }
    rc = dyad_shared_flock (mod_ctx->ctx, fd, &shared_lock);
    if (DYAD_IS_ERROR (rc)) {
        goto fetch_error;
    }
    file_size = get_file_size (fd);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: file %s has size %zd", fullpath, file_size);
    // clang-format off
#ifdef DYAD_ENABLE_UCX_DTL
    // For UCX RMA, prepend file_size so the consumer can find the data boundary
    // in the shared buffer without an extra RMA call.
    const size_t buf_offset = sizeof (file_size);
#else
    const size_t buf_offset = 0;
#endif
    // clang-format on
    rc = mod_ctx->ctx->dtl_handle->get_buffer (mod_ctx->ctx, file_size, (void **)&inbuf);
#ifdef DYAD_ENABLE_UCX_DTL
    memcpy (inbuf, &file_size, sizeof (file_size));
#endif
    if (file_size > 0l) {
        if (file_size < DYAD_POSIX_TRANSFER_GRANULARITY) {
            inlen = read (fd, inbuf + buf_offset, file_size);
        } else {
            ssize_t read_data = 0;
            int granularity = DYAD_POSIX_TRANSFER_GRANULARITY;
            while (read_data < file_size) {
                ssize_t read_size =
                    (file_size - read_data) > granularity ? granularity : (file_size - read_data);
                inlen = read (fd, inbuf + buf_offset + read_data, read_size);
                DYAD_LOG_DEBUG (mod_ctx->ctx,
                                "DYAD_MOD: reading file %s with bytes %zd of %zd",
                                fullpath,
                                read_size,
                                inlen);
                if (inlen <= 0) {
                    DYAD_LOG_ERROR (mod_ctx->ctx,
                                    "DYAD_MOD: Failed to load file \"%s\" only read %zd of %zd of "
                                    "%zd. with code %d:%s.",
                                    fullpath,
                                    inlen,
                                    read_size,
                                    file_size,
                                    errno,
                                    strerror (errno));
                    goto fetch_error;
                }
                read_data += inlen;
            }
            inlen = read_data;
        }
        if (inlen != file_size) {
            DYAD_LOG_ERROR (mod_ctx->ctx,
                            "DYAD_MOD: Failed to load file \"%s\" only read %zd of %zd. with code "
                            "%d:%s.",
                            fullpath,
                            inlen,
                            file_size,
                            errno,
                            strerror (errno));
            goto fetch_error;
        }
        inlen = file_size + (ssize_t)buf_offset;
        DYAD_C_FUNCTION_UPDATE_INT ("file_size", file_size);
        dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
        close (fd);
        // DYAD_LOG_DEBUG (mod_ctx->ctx, "Is inbuf NULL? -> %i", (int)(inbuf == NULL));
        DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Establish DTL connection with consumer");
        rc = mod_ctx->ctx->dtl_handle->establish_connection (mod_ctx->ctx);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx,
                            "DYAD_MOD: Could not establish DTL connection with client");
            errno = ECONNREFUSED;
            goto fetch_error_wo_flock;
        }
        DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Send file to consumer with DTL");
        rc = mod_ctx->ctx->dtl_handle->send (mod_ctx->ctx, inbuf, inlen);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not send data to client via DTL\n");
            errno = ECOMM;
            goto fetch_error_wo_flock;
        }
        DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Close DTL connection with consumer");
        mod_ctx->ctx->dtl_handle->close_connection (mod_ctx->ctx);
        mod_ctx->ctx->dtl_handle->return_buffer (mod_ctx->ctx, (void **)&inbuf);
    } else {
        goto fetch_error;
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx,
                    "DYAD_MOD: Close RPC message stream with an ENODATA (%d) message",
                    ENODATA);
    if (flux_respond_error (h, msg, ENODATA, NULL) < 0) {
        DYAD_LOG_DEBUG (mod_ctx->ctx,
                        "DYAD_MOD: %s: flux_respond_error with ENODATA failed\n",
                        __func__);
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Finished %s module invocation\n", DYAD_DTL_RPC_NAME);
    goto end_fetch_cb;

fetch_error:;
    dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
    close (fd);

fetch_error_wo_flock:;
    DYAD_LOG_ERROR (mod_ctx->ctx,
                    "DYAD_MOD: Close RPC message stream with an error (errno = %d)\n",
                    errno);
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: %s: flux_respond_error", __func__);
    }
    errno = saved_errno;
    DYAD_C_FUNCTION_END ();
    return;

end_fetch_cb:;
    errno = saved_errno;
    DYAD_C_FUNCTION_END ();
    return;
}

/**
 * @brief Flux message handler table for the DYAD module.
 *
 * @details
 * Registers @c dyad_fetch_request_cb as the handler for all incoming
 * @c FLUX_MSGTYPE_REQUEST messages addressed to @c DYAD_DTL_RPC_NAME.
 * This is the single RPC endpoint exposed by the DYAD module — consumers
 * send file fetch requests to this name on the producer's broker, and the
 * reactor dispatches them to @c dyad_fetch_request_cb.
 * @c DYAD_DTL_RPC_NAME is defined as "dyad.fetch"
 *
 * Passed to @c flux_msg_handler_addvec() in @c mod_main() and terminated
 * by @c FLUX_MSGHANDLER_TABLE_END as required by the Flux API.
 */
static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, DYAD_DTL_RPC_NAME, dyad_fetch_request_cb, 0},
     FLUX_MSGHANDLER_TABLE_END};

static void show_help (void)
{
    DYAD_LOG_STDOUT ("dyad module options and arguments\n");
    DYAD_LOG_STDOUT ("    -h, --help:  Show help.\n");
    DYAD_LOG_STDOUT ("    -d, --debug: Enable debugging log message.\n");
    DYAD_LOG_STDOUT (
        "    -m, --mode:  DTL mode. Need an argument.\n"
        "                 Either 'FLUX_RPC' (default) or 'UCX'.\n");
    DYAD_LOG_STDOUT (
        "    -i, --info_log: Specify the file into which to redirect\n"
        "                    info logging. Does nothing if DYAD was not\n"
        "                    configured with '-DDYAD_LOGGER=PRINTF'.\n"
        "                    Need a filename as an argument.\n");
    DYAD_LOG_STDOUT (
        "    -e, --error_log: Specify the file into which to redirect\n"
        "                     error logging. Does nothing if DYAD was\n"
        "                     not configured with '-DDYAD_LOGGER=PRINTF'\n"
        "                     Need a filename as an argument.\n");
}

/**
 * @brief Parsed command-line options for the DYAD Flux module.
 */
struct opt_parse_out {
    const char *prod_managed_path;  ///< Producer-managed directory path, or @c NULL.
    const char *dtl_mode;           ///< DTL mode string, or @c NULL for default.
    bool debug;                     ///< Whether debug logging is enabled.
    bool showed_help;               ///< Whether @c -h was passed and help was shown.
};

typedef struct opt_parse_out opt_parse_out_t;

/**
 * @brief Parses command-line arguments passed to the DYAD Flux module.
 *
 * @details
 * Parses @p argc / @p argv using @c getopt_long(). Because Flux module
 * argument vectors do not include the executable name in @c argv[0] (unlike
 * standard @c main()), a synthetic @c _argv is constructed with a @c NULL
 * dummy first element so that @c getopt() works correctly. @c optind is
 * reset to 1 before each call to handle repeated invocations.
 *
 * Recognized options:
 *  - @c -h / @c --help        Sets @c opt->showed_help and returns.
 *  - @c -d / @c --debug       Sets @c opt->debug.
 *  - @c -m / @c --mode        Sets @c opt->dtl_mode.
 *  - @c -i / @c --info_log    Redirects info log output to a per-rank file.
 *  - @c -e / @c --error_log   Redirects error log output to a per-rank file.
 *
 * Any remaining non-option argument is treated as the producer-managed
 * directory path and stored in @c opt->prod_managed_path.
 *
 * Log redirection options have no effect unless DYAD was built with
 * @c -DDYAD_LOGGER=PRINTF.
 *
 * @param[out] opt          Output structure populated with parsed options.
 *                          Must not be @c NULL.
 * @param[in]  broker_rank  Broker rank, used to name per-rank log files.
 * @param[in]  argc         Number of module arguments.
 * @param[in]  argv         Module argument strings.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK      Parsing succeeded.
 * @retval DYAD_RC_SYSFAIL An unrecognized option was encountered.
 */
int opt_parse (opt_parse_out_t *restrict opt,
               const unsigned broker_rank,
               int argc,
               char **restrict argv)
{
#ifndef DYAD_LOGGER_NO_LOG
    char log_file_name[PATH_MAX + 1] = {'\0'};
    char err_file_name[PATH_MAX + 1] = {'\0'};
    sprintf (log_file_name, "dyad_mod_%u.out", broker_rank);
    sprintf (err_file_name, "dyad_mod_%d.err", broker_rank);
#endif  // DYAD_LOGGER_NO_LOG

    int rc = DYAD_RC_OK;
    char *prod_managed_path = NULL;

    if (opt == NULL)
        return rc;

    // In case getopt() is called multiple times, e.g.,
    // when doing "flux module load dyad.so -h"
    // optind must be reset to 1.
    // Otherwise, getopt() may cause crash.
    // Note, getopt() assumes the first argument, i.e.,
    // argv[0] to be the executable name, so it starts
    // checking from optind = 1.
    // since Flux module argv doesn't contain the executable
    // name in its first argument, we need to create a dummy
    // _argc and _argv here for getopt() to work properly.
    extern int optind;
    optind = 1;
    int _argc = argc + 1;
    char **_argv = malloc (sizeof (char *) * _argc);
    _argv[0] = NULL;
    for (int i = 1; i < _argc; i++) {
        // we will reuse the same string in argv[].
        _argv[i] = argv[i - 1];
    }

    static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                           {"debug", no_argument, 0, 'd'},
                                           {"mode", required_argument, 0, 'm'},
                                           {"info_log", required_argument, 0, 'i'},
                                           {"error_log", required_argument, 0, 'e'},
                                           {0, 0, 0, 0}};

    int c;
    while ((c = getopt_long (_argc, _argv, "hdm:i:e:", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                show_help ();
                // set this to true, so we later we will directly
                // return without loading the Flux module.
                opt->showed_help = true;
                break;
            case 'd':
                DYAD_LOG_STDERR ("DYAD_MOD: 'debug' option -d\n");
                opt->debug = true;
                break;
            case 'm':
                // If the DTL is already initialized and it is set to the same
                // mode as the option, then skip reinitializing
                DYAD_LOG_STDERR ("DYAD_MOD: DTL 'mode' option -m with value `%s'\n", optarg);
                opt->dtl_mode = optarg;
                // TODO: check if the user specified dtl_mode is valid.
                break;
            case 'i':
#ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_STDERR ("DYAD_MOD: 'info_log' option -i with value `%s'\n", optarg);
                sprintf (log_file_name, "%s_%d.out", optarg, broker_rank);
#endif  // DYAD_LOGGER_NO_LOG
                break;
            case 'e':
#ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_STDERR ("DYAD_MOD: 'error_log' option -e with value `%s'\n", optarg);
                sprintf (err_file_name, "%s_%d.err", optarg, broker_rank);
#endif  // DYAD_LOGGER_NO_LOG
                break;
            case '?':
                /* getopt_long already printed an error message. */
                break;
            default:
                DYAD_LOG_STDERR ("DYAD_MOD: option parsing failed %d\n", c);
                free (_argv);
                return DYAD_RC_SYSFAIL;
        }
    }

#ifndef DYAD_LOGGER_NO_LOG
    DYAD_LOG_STDOUT_REDIRECT (log_file_name);
    DYAD_LOG_STDERR_REDIRECT (err_file_name);
#endif  // DYAD_LOGGER_NO_LOG

    // Retrive the remaining command line argument (not options).
    // it is expected to be the producer managed directory
    while (optind < _argc) {
        prod_managed_path = _argv[optind++];
    }
    opt->prod_managed_path = prod_managed_path;

    free (_argv);
    return DYAD_RC_OK;
}

/**
 * @brief Initializes the DYAD context for the Flux module.
 *
 * @details
 * Configures the DYAD context for use as a Flux module (producer side),
 * bridging command-line arguments and environment variables before delegating
 * to @c dyad_ctx_init().
 *
 * Configuration is layered in the following order of precedence:
 *  1. Environment variables provide the baseline configuration.
 *  2. Command-line arguments in @p opt override environment variables by
 *     calling @c setenv() before @c dyad_ctx_init() is invoked.
 *
 * Specifically:
 *  - If @c opt->prod_managed_path is set, it is written to
 *    @c DYAD_PATH_PRODUCER_ENV and the directory is created if it does
 *    not already exist.
 *  - If @c opt->dtl_mode is set, it is written to @c DYAD_DTL_MODE_ENV.
 *  - If @c DYAD_KVS_NAMESPACE is not set in the environment, a dummy
 *    value is written to allow @c dyad_ctx_init() to proceed. This is
 *    a known limitation (see TODO in source).
 *
 * After environment setup, calls @c dyad_ctx_init() with
 * @c DYAD_COMM_SEND and the provided Flux handle @p h, then retrieves
 * the initialized context and stores it in @c mod_ctx->ctx. The Flux
 * handle and debug flag are also applied directly to the context after
 * initialization.
 *
 * @param[in] opt  Parsed command-line options. Must not be @c NULL.
 *                 Contains optional overrides for the producer path and
 *                 DTL mode.
 * @param[in] h    Flux handle provided by the broker to @c mod_main().
 *                 Must not be @c NULL. Stored directly in the context
 *                 after initialization.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK     The context was successfully initialized.
 * @retval DYAD_RC_NOCTX  @p opt, @p h, or the module context is @c NULL,
 *                        or @c dyad_ctx_init() failed to initialize the
 *                        context or its DTL handle.
 *
 * @note Unlike the C GOTCHA wrapper and C++ stream paths, the Flux module
 *       initializes with @c DYAD_COMM_SEND (producer) rather than
 *       @c DYAD_COMM_RECV (consumer), and adopts the broker-provided Flux
 *       handle rather than opening its own.
 */
dyad_rc_t dyad_module_ctx_init (const opt_parse_out_t *opt, flux_t *h)
{
    // get DYAD Flux module
    dyad_mod_ctx_t *mod_ctx = get_mod_ctx (h);

    if (mod_ctx == NULL || opt == NULL || h == NULL) {
        return DYAD_RC_NOCTX;
    }

    // DYAD can be configured via environment variables
    // and users can override the settings through command
    // line arguments.
    if (opt->prod_managed_path) {
        setenv (DYAD_PATH_PRODUCER_ENV, opt->prod_managed_path, 1);
        const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
        mkdir_as_needed (opt->prod_managed_path, m);
        DYAD_LOG_STDOUT ("DYAD_MOD: Loading DYAD Module with Path %s\n", opt->prod_managed_path);
    }

    if (opt->dtl_mode) {
        setenv (DYAD_DTL_MODE_ENV, opt->dtl_mode, 1);
        DYAD_LOG_STDOUT ("DYAD_MOD: DTL mode option set. Setting env %s=%s\n",
                         DYAD_DTL_MODE_ENV,
                         opt->dtl_mode);
    }

    char *kvs_namespace = getenv ("DYAD_KVS_NAMESPACE");
    if (kvs_namespace != NULL) {
        DYAD_LOG_STDOUT ("DYAD_MOD: DYAD_KVS_NAMESPACE is set to `%s'\n", kvs_namespace);
    } else {
        // Required so that dyad_ctx_init can pass
        // TODO: figure out a better for this.
        setenv (DYAD_KVS_NAMESPACE_ENV, "dyad_module_dummy_env", 1);
    }

    // Initialize DYAD context
    dyad_ctx_init (DYAD_COMM_SEND, h);
    dyad_ctx_t *ctx = dyad_ctx_get ();
    mod_ctx->ctx = ctx;

    if (ctx == NULL) {
        DYAD_LOG_STDERR ("DYAD_MOD: dyad_ctx_init() failed!\n");
        return DYAD_RC_NOCTX;
    }
    ctx->h = h;
    ctx->debug = opt->debug;

    if (ctx->dtl_handle == NULL) {
        DYAD_LOG_STDERR ("DYAD_MOD: dyad_ctx_init() failed to initialize DTL!\n");
        return DYAD_RC_NOCTX;
    }

    return DYAD_RC_OK;
}

/**
 * @brief Entry point for the DYAD Flux module, invoked in a new broker
 *        thread when the module is loaded.
 *
 * @details
 * Called by the Flux broker when the DYAD module is loaded. The @p h
 * handle provides direct communication with the broker over shared memory.
 * When @c mod_main() returns, the thread is terminated and the module is
 * unloaded.
 *
 * Performs the following steps in order:
 *  1. Validates the Flux handle @p h.
 *  2. Retrieves the module context via @c get_mod_ctx().
 *  3. Parses command-line arguments via @c opt_parse(). If @c -h was
 *     passed, prints help and returns immediately.
 *  4. Initializes the DYAD context via @c dyad_module_ctx_init(), which
 *     applies command-line overrides to environment variables before
 *     calling @c dyad_ctx_init().
 *  5. Registers Flux message handlers from @c htab via
 *     @c flux_msg_handler_addvec().
 *  6. Runs the Flux reactor loop via @c flux_reactor_run(), blocking
 *     until the module is unloaded.
 *
 * On any error, jumps to @c mod_error and returns @c EXIT_FAILURE.
 * On success or after printing help, jumps to @c mod_done and returns
 * @c EXIT_SUCCESS.
 *
 * @param[in] h     Flux handle provided by the broker over shared memory.
 *                  Must not be @c NULL.
 * @param[in] argc  Number of command-line arguments passed to the module
 *                  by the broker.
 * @param[in] argv  Command-line argument strings derived from the free
 *                  arguments on the @c flux module load command line.
 *                  Parsed by @c opt_parse() to extract the producer path,
 *                  DTL mode, and debug flag.
 *
 * @return int
 * @retval EXIT_SUCCESS  The module ran and exited cleanly, or @c -h was
 *                       passed and help was displayed.
 * @retval EXIT_FAILURE  Any step in the initialization or reactor loop
 *                       failed.
 *
 * @note If @c DYAD_PROFILER_DFTRACER is defined, initializes the DFTracer
 *       profiler using the broker rank as the process ID before any other
 *       setup.
 */
DYAD_DLL_EXPORTED int mod_main (flux_t *h, int argc, char **argv)
{
    /** If this not a singleton init where the duplicate init is noop,
     *  this can cause an error. cpp-logger is singlton. flux logger is
     *  not initialized in dyad. So, it is ok for the choices we have
     *  for now, but it needs to be revisited when adding a new logger
     *  later. If is similar for profiler. That is why dftracer is
     *  initialized explicitly to avoid false impression that other
     *  non-singleton profiler can be initialized at the same places.
     */
    DYAD_LOGGER_INIT ();
    DYAD_LOG_STDOUT ("DYAD_MOD: Loading mod_main\n");
    dyad_mod_ctx_t *mod_ctx = NULL;
    uint32_t broker_rank;
    flux_get_rank (h, &broker_rank);

#ifdef DYAD_PROFILER_DFTRACER
    /** Note that dftracer is initialized in dyad_init () which is called via
     *  dyad_module_ctx_init () below. This is only ok because dftracer is
     *  singleton and has guard against duplicate intialization.
     *  The subtle issue is that dyad_init () calls
     *  DFTRACER_C_INIT_NO_BIND (log_file, NULL, NULL) which will be ignored.
     */
    int pid = broker_rank;
    DFTRACER_C_INIT (NULL, NULL, &pid);
#endif

    if (!h) {
        DYAD_LOG_STDERR ("DYAD_MOD: Failed to get flux handle\n");
        goto mod_done;
    }

    mod_ctx = get_mod_ctx (h);

    opt_parse_out_t opt = {NULL, NULL, false, false};

    if (DYAD_IS_ERROR (opt_parse (&opt, broker_rank, argc, argv))) {
        DYAD_LOG_STDERR ("DYAD_MOD: Cannot parse command line arguments\n");
        goto mod_error;
    }
    // the service was invoked with "-h"
    // then we return directly after printing out help message
    if (opt.showed_help) {
        goto mod_done;
    }

    // initialize mod_ctx->ctx, which is the dyad context
    if (DYAD_IS_ERROR (dyad_module_ctx_init (&opt, h))) {
        goto mod_error;
    }
    /** This is not just for dftracer but an alias for other profiler calls as well.
     *  That is why comes after the potential profiler initialization, which can
     *  happen during dyad_ctx initialization, i.e., dyad_init ().
     */
    DYAD_C_FUNCTION_START ();

    if (flux_msg_handler_addvec (mod_ctx->ctx->h, htab, (void *)h, &mod_ctx->handlers) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: flux_msg_handler_addvec: %s\n", strerror (errno));
        goto mod_error;
    }

    if (flux_reactor_run (flux_get_reactor (mod_ctx->ctx->h), 0) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: flux_reactor_run: %s\n", strerror (errno));
        goto mod_error;
    }
    DYAD_LOG_STDOUT ("DYAD_MOD: Finished\n");
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
