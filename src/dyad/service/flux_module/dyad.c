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
#include <dyad/utils/range_cache.h>
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
#include <pthread.h>
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
/**
 * @brief One in-flight @c dyad.fetch_range request handed off from the
 *        reactor thread to the worker-thread pool.
 *
 * @details
 * Populated by @c dyad_fetch_range_request_cb() on the reactor thread
 * (everything needed to do the request's file I/O without touching the DTL
 * or Flux state again), then filled in by a worker thread
 * (@c dyad_fetch_worker_process()) with the outcome. Freed by whichever
 * code path finishes the request -- either a worker thread directly
 * (Margo, @c send_detached_is_thread_safe) or the reactor-thread completion
 * callback (Flux RPC).
 */
struct dyad_fetch_work_item {
    char fullpath[PATH_MAX + 1];
    char origin_fullpath[PATH_MAX + 1];
    bool has_origin;
    size_t offset;
    size_t length;
    void *inbuf;      ///< Allocated via dtl_handle->get_buffer() on the reactor thread.
    void *req_state;  ///< From dtl_handle->rpc_detach_request().
    flux_msg_t *msg;  ///< Incref'd; used only for flux_respond_error() on failure.
    bool send_detached_is_thread_safe;
    struct dyad_mod_ctx *mod_ctx;
    // Outcome, filled in by the worker:
    dyad_rc_t rc;
    int saved_errno;
    ssize_t range_len;
    struct dyad_fetch_work_item *next;
};

typedef struct dyad_mod_ctx {
    flux_msg_handler_t **handlers;  ///< Flux message handler table.
    dyad_ctx_t *ctx;                ///< DYAD context for this module instance.

    // --- Worker-thread pool for dyad_fetch_range_request_cb()'s blocking
    // I/O (and, for the Margo DTL, the RDMA send itself). See
    // DYAD_FETCH_WORKER_THREADS_ENV. NULL/0/unset until mod_main() creates
    // the pool; dyad_fetch_range_request_cb() falls back to the original
    // fully-synchronous inline path whenever the active DTL backend's
    // rpc_detach_request is NULL (currently UCX), so these fields are only
    // ever touched when Flux RPC or Margo is active.
    int num_fetch_workers;
    pthread_t *fetch_workers;
    pthread_mutex_t work_mutex;
    pthread_cond_t work_cond;
    struct dyad_fetch_work_item *work_head;
    struct dyad_fetch_work_item *work_tail;
    bool workers_shutdown;

    // --- Completion hand-back to the reactor thread, needed only for
    // DTL backends where send_detached_is_thread_safe is false (Flux RPC):
    // flux_respond_raw()/flux_respond_error() touch the module's flux_t
    // handle, which must only be touched from the thread that owns its
    // reactor. A worker thread finishing such a request pushes it onto
    // completion_head/tail and writes one byte to completion_pipe[1]; the
    // reactor-thread fd-watcher (completion_watcher) drains the pipe and
    // finishes each item on completion_head/tail.
    int completion_pipe[2];
    flux_watcher_t *completion_watcher;
    pthread_mutex_t completion_mutex;
    struct dyad_fetch_work_item *completion_head;
    struct dyad_fetch_work_item *completion_tail;
} dyad_mod_ctx_t;

const struct dyad_mod_ctx dyad_mod_ctx_default = {0};

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
    flux_msg_handler_delvec (mod_ctx->handlers);
    if (mod_ctx->ctx) {
        dyad_ctx_fini ();
        mod_ctx->ctx = NULL;
    }
    // The worker pool (if created) is already stopped and joined by
    // mod_main() before it returns, and completion_watcher already
    // destroyed there too -- this only frees the primitives themselves.
    free (mod_ctx->fetch_workers);
    pthread_mutex_destroy (&mod_ctx->work_mutex);
    pthread_cond_destroy (&mod_ctx->work_cond);
    pthread_mutex_destroy (&mod_ctx->completion_mutex);
    if (mod_ctx->completion_pipe[0] != -1) {
        close (mod_ctx->completion_pipe[0]);
    }
    if (mod_ctx->completion_pipe[1] != -1) {
        close (mod_ctx->completion_pipe[1]);
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
        mod_ctx->num_fetch_workers = 0;
        mod_ctx->fetch_workers = NULL;
        mod_ctx->work_head = NULL;
        mod_ctx->work_tail = NULL;
        mod_ctx->workers_shutdown = false;
        mod_ctx->completion_pipe[0] = -1;
        mod_ctx->completion_pipe[1] = -1;
        mod_ctx->completion_watcher = NULL;
        mod_ctx->completion_head = NULL;
        mod_ctx->completion_tail = NULL;
        pthread_mutex_init (&mod_ctx->work_mutex, NULL);
        pthread_cond_init (&mod_ctx->work_cond, NULL);
        pthread_mutex_init (&mod_ctx->completion_mutex, NULL);

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
    bool request_unpacked = false;
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
    request_unpacked = true;
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
    {
        bool has_range_cache = false;
        bool range_cache_complete = true;

        rc = dyad_range_cache_is_complete (mod_ctx->ctx,
                                           fullpath,
                                           &has_range_cache,
                                           &range_cache_complete);
        if (DYAD_IS_ERROR (rc)) {
            errno = EIO;
            goto fetch_error_wo_flock;
        }
        if (has_range_cache && !range_cache_complete) {
            char origin_fullpath[PATH_MAX + 1] = {'\0'};
            struct stat origin_st;

            if (mod_ctx->ctx->origin_path == NULL) {
                errno = ENODATA;
                goto fetch_error_wo_flock;
            }
            strncpy (origin_fullpath, mod_ctx->ctx->origin_path, PATH_MAX - 1);
            concat_str (origin_fullpath, upath, "/", PATH_MAX);
            if (stat (origin_fullpath, &origin_st) != 0 || origin_st.st_size <= 0
                || DYAD_IS_ERROR (dyad_range_cache_ensure (mod_ctx->ctx,
                                                            fullpath,
                                                            origin_fullpath,
                                                            0ul,
                                                            (size_t)origin_st.st_size))) {
                errno = EIO;
                goto fetch_error_wo_flock;
            }
        }
    }
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
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not allocate DTL buffer for fetch");
        errno = ENOMEM;
        goto fetch_error;
    }
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
    if (request_unpacked && mod_ctx->ctx->dtl_handle->mode != DYAD_DTL_FLUX_RPC
        && !DYAD_IS_ERROR (mod_ctx->ctx->dtl_handle->establish_connection (mod_ctx->ctx))) {
        mod_ctx->ctx->dtl_handle->send (mod_ctx->ctx, NULL, DYAD_DTL_SEND_ERROR);
        mod_ctx->ctx->dtl_handle->close_connection (mod_ctx->ctx);
    }
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

// =========================================================================
// Worker-thread pool for dyad_fetch_range_request_cb()'s blocking I/O.
//
// dyad_fetch_range_request_cb() (below) does the cheap, message-parsing
// part inline on the reactor thread, then -- for DTL backends that support
// it (Flux RPC, Margo; see dyad_dtl::rpc_detach_request) -- hands the rest
// of the request off to one of these worker threads as a
// dyad_fetch_work_item, so concurrent requests to this broker don't
// serialize behind whichever one happened to arrive first.
//
// Every completed item, regardless of backend, ends up on
// completion_head/tail and gets a wakeup byte written to completion_pipe --
// even for backends where dyad_fetch_worker_finish() already did the data
// send directly on the worker thread (send_detached_is_thread_safe) --
// because the trailing flux_respond_error(..., ENODATA, ...) call that
// closes the streaming RPC always touches the module's flux_t handle, and
// must therefore always run on the reactor thread
// (dyad_fetch_completion_watcher_cb()), regardless of backend.
// =========================================================================

// Frees everything a work item owns. rpc_send_detached()/rpc_abort_detached()
// must already have been called (req_state consumed) before this runs.
static void dyad_fetch_work_item_destroy (dyad_mod_ctx_t *mod_ctx,
                                          struct dyad_fetch_work_item *item)
{
    if (item->inbuf != NULL) {
        mod_ctx->ctx->dtl_handle->return_buffer (mod_ctx->ctx, (void **)&item->inbuf);
    }
    if (item->msg != NULL) {
        flux_msg_decref (item->msg);
    }
    free (item);
}

// Runs on the reactor thread (registered as a flux_fd_watcher_create()
// callback on completion_pipe's read end): drains the pipe, then finishes
// every item on the completion queue -- the data send, for backends where
// it wasn't already done on a worker thread (item->req_state still set),
// and always the trailing flux_respond_error() that closes the streaming
// RPC.
static void dyad_fetch_completion_watcher_cb (flux_reactor_t *r,
                                              flux_watcher_t *w,
                                              int revents,
                                              void *arg)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)arg;
    dyad_ctx_t *ctx = mod_ctx->ctx;
    char drain_buf[256];
    ssize_t n;
    struct dyad_fetch_work_item *items = NULL;

    do {
        n = read (mod_ctx->completion_pipe[0], drain_buf, sizeof (drain_buf));
    } while (n > 0);

    pthread_mutex_lock (&mod_ctx->completion_mutex);
    items = mod_ctx->completion_head;
    mod_ctx->completion_head = NULL;
    mod_ctx->completion_tail = NULL;
    pthread_mutex_unlock (&mod_ctx->completion_mutex);

    while (items != NULL) {
        struct dyad_fetch_work_item *item = items;
        items = items->next;
        item->next = NULL;

        if (!DYAD_IS_ERROR (item->rc)) {
            if (item->req_state != NULL) {
                dyad_rc_t rc = ctx->dtl_handle->rpc_send_detached (ctx,
                                                                   item->req_state,
                                                                   item->inbuf,
                                                                   (size_t)item->range_len);
                if (DYAD_IS_ERROR (rc)) {
                    DYAD_LOG_ERROR (ctx,
                                    "DYAD_MOD: rpc_send_detached failed for \"%s\".",
                                    item->fullpath);
                }
            }
            if (flux_respond_error (ctx->h, item->msg, ENODATA, NULL) < 0) {
                DYAD_LOG_DEBUG (ctx,
                                "DYAD_MOD: %s: flux_respond_error with ENODATA failed\n",
                                __func__);
            }
        } else {
            if (item->req_state != NULL) {
                ctx->dtl_handle->rpc_send_detached (ctx,
                                                    item->req_state,
                                                    NULL,
                                                    DYAD_DTL_SEND_ERROR);
                item->req_state = NULL;
            }
            if (flux_respond_error (ctx->h, item->msg, item->saved_errno, NULL) < 0) {
                DYAD_LOG_ERROR (ctx, "DYAD_MOD: %s: flux_respond_error", __func__);
            }
        }
        dyad_fetch_work_item_destroy (mod_ctx, item);
    }
}

// Hands a finished (or failed) item to the completion queue and wakes the
// reactor thread. For thread-safe backends (Margo), the data send has
// already happened by the time this is called (item->req_state cleared);
// for others (Flux RPC) it's still pending and item->req_state is left set
// for dyad_fetch_completion_watcher_cb() to finish.
static void dyad_fetch_worker_finish (dyad_mod_ctx_t *mod_ctx, struct dyad_fetch_work_item *item)
{
    dyad_ctx_t *ctx = mod_ctx->ctx;
    char byte = 1;
    ssize_t written;

    if (item->send_detached_is_thread_safe) {
        if (!DYAD_IS_ERROR (item->rc)) {
            dyad_rc_t rc = ctx->dtl_handle->rpc_send_detached (ctx,
                                                               item->req_state,
                                                               item->inbuf,
                                                               (size_t)item->range_len);
            if (DYAD_IS_ERROR (rc)) {
                item->rc = rc;
                item->saved_errno = ECOMM;
            }
        } else {
            ctx->dtl_handle->rpc_abort_detached (ctx, item->req_state);
        }
        item->req_state = NULL;  // consumed -- the completion callback must not touch it
    }

    pthread_mutex_lock (&mod_ctx->completion_mutex);
    item->next = NULL;
    if (mod_ctx->completion_tail == NULL) {
        mod_ctx->completion_head = item;
    } else {
        mod_ctx->completion_tail->next = item;
    }
    mod_ctx->completion_tail = item;
    pthread_mutex_unlock (&mod_ctx->completion_mutex);

    do {
        written = write (mod_ctx->completion_pipe[1], &byte, 1);
    } while (written < 0 && errno == EINTR);
    // A lost wakeup (e.g. EAGAIN on a full pipe -- vanishingly unlikely for
    // single-byte writes) is harmless: the queue is drained in full on
    // every wakeup, so any later completion's wakeup picks this item up
    // too.
}

// Runs entirely on a worker thread: the blocking file I/O that used to be
// inline in dyad_fetch_range_request_cb(). Touches no Flux/DTL state
// directly -- only item->req_state (opaque, backend-specific) via
// dyad_fetch_worker_finish() above.
static void dyad_fetch_worker_process (dyad_mod_ctx_t *mod_ctx, struct dyad_fetch_work_item *item)
{
    dyad_ctx_t *ctx = mod_ctx->ctx;
    int fd = -1;
    ssize_t file_size = 0l;
    ssize_t range_len = 0l;
    dyad_rc_t rc = DYAD_RC_OK;
    int saved_errno = 0;
    struct flock shared_lock;

    if (item->has_origin) {
        rc = dyad_range_cache_ensure (ctx,
                                      item->fullpath,
                                      item->origin_fullpath,
                                      item->offset,
                                      item->length);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_STDERR ("DYAD_MOD: dyad_range_cache_ensure failed for file \"%s\".\n",
                             item->fullpath);
            saved_errno = EIO;
            goto worker_error_no_fd;
        }
    }

    fd = open (item->fullpath, O_RDONLY);
    if (fd < 0) {
        DYAD_LOG_STDERR ("DYAD_MOD: Failed to open file \"%s\".\n", item->fullpath);
        saved_errno = errno;
        goto worker_error_no_fd;
    }
    rc = dyad_shared_flock (ctx, fd, &shared_lock);
    if (DYAD_IS_ERROR (rc)) {
        saved_errno = errno;
        goto worker_error_fd;
    }
    file_size = get_file_size (fd);
    // Clamp against the real file size for safety -- offset/length come
    // from the consumer's own index and should already be valid, but a
    // stale/corrupt index must not turn into an out-of-bounds pread().
    if ((ssize_t)item->offset >= file_size) {
        range_len = 0;
    } else if ((ssize_t)(item->offset + item->length) > file_size) {
        range_len = file_size - (ssize_t)item->offset;
    } else {
        range_len = (ssize_t)item->length;
    }

    if (range_len > 0l) {
        ssize_t read_data = 0;
        int granularity = DYAD_POSIX_TRANSFER_GRANULARITY;
        while (read_data < range_len) {
            ssize_t read_size =
                (range_len - read_data) > granularity ? granularity : (range_len - read_data);
            ssize_t inlen = pread (fd,
                                   (char *)item->inbuf + read_data,
                                   (size_t)read_size,
                                   (off_t)(item->offset + (size_t)read_data));
            if (inlen <= 0) {
                DYAD_LOG_STDERR ("DYAD_MOD: Failed to load range of file \"%s\" (errno %d: %s).\n",
                                 item->fullpath,
                                 errno,
                                 strerror (errno));
                saved_errno = errno ? errno : EIO;
                dyad_release_flock (ctx, fd, &shared_lock);
                goto worker_error_fd;
            }
            read_data += inlen;
        }
    }
    dyad_release_flock (ctx, fd, &shared_lock);
    close (fd);

    item->rc = DYAD_RC_OK;
    item->range_len = range_len;
    dyad_fetch_worker_finish (mod_ctx, item);
    return;

worker_error_fd:;
    close (fd);
worker_error_no_fd:;
    item->rc = DYAD_RC_BADFIO;
    item->saved_errno = saved_errno;
    dyad_fetch_worker_finish (mod_ctx, item);
}

// Worker-thread entry point: pops items off the shared work queue and
// processes them until told to shut down.
static void *dyad_fetch_worker_main (void *arg)
{
    dyad_mod_ctx_t *mod_ctx = (dyad_mod_ctx_t *)arg;
    for (;;) {
        struct dyad_fetch_work_item *item = NULL;
        pthread_mutex_lock (&mod_ctx->work_mutex);
        while (mod_ctx->work_head == NULL && !mod_ctx->workers_shutdown) {
            pthread_cond_wait (&mod_ctx->work_cond, &mod_ctx->work_mutex);
        }
        if (mod_ctx->work_head == NULL && mod_ctx->workers_shutdown) {
            pthread_mutex_unlock (&mod_ctx->work_mutex);
            break;
        }
        item = mod_ctx->work_head;
        mod_ctx->work_head = item->next;
        if (mod_ctx->work_head == NULL) {
            mod_ctx->work_tail = NULL;
        }
        pthread_mutex_unlock (&mod_ctx->work_mutex);
        item->next = NULL;

        dyad_fetch_worker_process (mod_ctx, item);
    }
    return NULL;
}

// Creates completion_pipe/completion_watcher and the fetch-worker pool
// (size from DYAD_FETCH_WORKER_THREADS_ENV, default 8). On any failure,
// leaves mod_ctx->num_fetch_workers at 0 -- dyad_fetch_range_request_cb()
// checks this and falls back to the fully-synchronous path, so a pool
// start failure degrades performance but is not fatal to the module.
static dyad_rc_t dyad_fetch_pool_start (dyad_mod_ctx_t *mod_ctx)
{
    int i = 0;
    int num_workers = 8;
    int flags = 0;
    const char *env_workers = getenv (DYAD_FETCH_WORKER_THREADS_ENV);
    if (env_workers != NULL) {
        int parsed = atoi (env_workers);
        if (parsed > 0) {
            num_workers = parsed;
        }
    }

    if (pipe (mod_ctx->completion_pipe) != 0) {
        DYAD_LOG_STDERR ("DYAD_MOD: pipe() failed for fetch completion queue: %s\n",
                         strerror (errno));
        return DYAD_RC_SYSFAIL;
    }
    // Non-blocking on both ends: the reactor thread must never block
    // draining an empty pipe, and a worker thread must never block writing
    // a single wakeup byte (see the "lost wakeup" comment in
    // dyad_fetch_worker_finish()).
    flags = fcntl (mod_ctx->completion_pipe[0], F_GETFL, 0);
    fcntl (mod_ctx->completion_pipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl (mod_ctx->completion_pipe[1], F_GETFL, 0);
    fcntl (mod_ctx->completion_pipe[1], F_SETFL, flags | O_NONBLOCK);

    mod_ctx->completion_watcher = flux_fd_watcher_create (flux_get_reactor (mod_ctx->ctx->h),
                                                          mod_ctx->completion_pipe[0],
                                                          FLUX_POLLIN,
                                                          dyad_fetch_completion_watcher_cb,
                                                          mod_ctx);
    if (mod_ctx->completion_watcher == NULL) {
        DYAD_LOG_STDERR ("DYAD_MOD: flux_fd_watcher_create() failed for fetch completion queue\n");
        return DYAD_RC_SYSFAIL;
    }
    flux_watcher_start (mod_ctx->completion_watcher);

    mod_ctx->fetch_workers = (pthread_t *)calloc ((size_t)num_workers, sizeof (pthread_t));
    if (mod_ctx->fetch_workers == NULL) {
        return DYAD_RC_SYSFAIL;
    }
    for (i = 0; i < num_workers; i++) {
        if (pthread_create (&mod_ctx->fetch_workers[i], NULL, dyad_fetch_worker_main, mod_ctx)
            != 0) {
            DYAD_LOG_STDERR ("DYAD_MOD: pthread_create() failed for fetch worker %d\n", i);
            break;
        }
    }
    mod_ctx->num_fetch_workers = i;
    if (mod_ctx->num_fetch_workers == 0) {
        return DYAD_RC_SYSFAIL;
    }
    DYAD_LOG_STDOUT ("DYAD_MOD: started %d fetch-worker thread(s)\n", mod_ctx->num_fetch_workers);
    return DYAD_RC_OK;
}

// Signals shutdown and joins all fetch-worker threads, then stops the
// completion watcher. Safe to call even if dyad_fetch_pool_start() was
// never called or failed before creating any threads.
static void dyad_fetch_pool_stop (dyad_mod_ctx_t *mod_ctx)
{
    int i;
    if (mod_ctx->fetch_workers == NULL) {
        return;
    }
    pthread_mutex_lock (&mod_ctx->work_mutex);
    mod_ctx->workers_shutdown = true;
    pthread_cond_broadcast (&mod_ctx->work_cond);
    pthread_mutex_unlock (&mod_ctx->work_mutex);

    for (i = 0; i < mod_ctx->num_fetch_workers; i++) {
        pthread_join (mod_ctx->fetch_workers[i], NULL);
    }
    if (mod_ctx->completion_watcher != NULL) {
        flux_watcher_stop (mod_ctx->completion_watcher);
        flux_watcher_destroy (mod_ctx->completion_watcher);
        mod_ctx->completion_watcher = NULL;
    }
}

/**
 * @brief Flux message handler callback that serves a byte range of a file
 *        to a consumer via RPC.
 *
 * @details
 * Registered as the handler for @c DYAD_DTL_RPC_RANGE_NAME requests in
 * @c htab. Parallel to @c dyad_fetch_request_cb(), for @c dyad_consume_range()
 * requests (FLUX_RPC and MARGO DTL modes only) — the existing whole-file
 * handler is untouched. Differs only in:
 *  - Unpacks @c upath, @c offset, and @c length via @c rpc_unpack_range()
 *    instead of just @c upath.
 *  - Reads only @c [offset, offset+length) from the file via @c pread()
 *    (clamped against the real file size) instead of the whole file via
 *    @c fstat()-sized sequential @c read().
 *  - Allocates/sends a buffer sized to @c length instead of the whole
 *    file size. No UCX file-size-prefix handling, since UCX does not
 *    implement @c rpc_unpack_range() (out of scope for byte-range fetch).
 *
 * @param[in] h    Flux handle for the broker.
 * @param[in] w    Flux message handler (unused directly).
 * @param[in] msg  Incoming Flux RPC message containing @c upath/@c offset/
 *                 @c length packed by the consumer.
 * @param[in] arg  Auxiliary argument (the Flux handle, passed as @c void*
 *                 from @c flux_msg_handler_addvec()).
 */
#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static void
dyad_fetch_range_request_cb (flux_t *h, flux_msg_handler_t *w, const flux_msg_t *msg, void *arg)
{
    DYAD_C_FUNCTION_START ();
    dyad_mod_ctx_t *mod_ctx = get_mod_ctx (h);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Launched callback for %s", DYAD_DTL_RPC_RANGE_NAME);
    ssize_t inlen = 0l;
    char *inbuf = NULL;
    int fd = -1;
    uint32_t userid = 0u;
    char *upath = NULL;
    char fullpath[PATH_MAX + 1] = {'\0'};
    int saved_errno = errno;
    size_t offset = 0;
    size_t length = 0;
    ssize_t file_size = 0l;
    ssize_t range_len = 0l;
    dyad_rc_t rc = 0;
    struct flock shared_lock;
    bool request_unpacked = false;
    bool request_detached = false;
    if (!flux_msg_is_streaming (msg)) {
        errno = EPROTO;
        goto fetch_range_error_wo_flock;
    }

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto fetch_range_error_wo_flock;

    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: unpacking ranged RPC message");

    rc = mod_ctx->ctx->dtl_handle->rpc_unpack_range (mod_ctx->ctx, msg, &upath, &offset, &length);

    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not unpack ranged message from client");
        errno = EPROTO;
        goto fetch_range_error_wo_flock;
    }
    request_unpacked = true;
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_LOG_DEBUG (mod_ctx->ctx,
                    "DYAD_MOD: requested user_path: %s, offset: %zu, length: %zu",
                    upath,
                    offset,
                    length);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: sending initial response to consumer");

    rc = mod_ctx->ctx->dtl_handle->rpc_respond (mod_ctx->ctx, msg);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not send primary RPC response to client");
        goto fetch_range_error_wo_flock;
    }

    strncpy (fullpath, mod_ctx->ctx->prod_managed_path, PATH_MAX - 1);
    concat_str (fullpath, upath, "/", PATH_MAX);
    DYAD_C_FUNCTION_UPDATE_STR ("fullpath", fullpath);

    if (mod_ctx->ctx->dtl_handle->rpc_detach_request != NULL && mod_ctx->num_fetch_workers > 0) {
        // Threaded path (Flux RPC or Margo, worker pool available): hand
        // the blocking I/O -- and, for thread-safe backends, the send
        // itself -- off to the worker-thread pool instead of doing it
        // inline on this reactor thread. See the worker-pool block above
        // dyad_fetch_range_request_cb() for the full design.
        struct dyad_fetch_work_item *item =
            (struct dyad_fetch_work_item *)calloc (1ul, sizeof (struct dyad_fetch_work_item));
        if (item == NULL) {
            DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not allocate fetch work item");
            errno = ENOMEM;
            goto fetch_range_error_wo_flock;
        }
        snprintf (item->fullpath, sizeof (item->fullpath), "%s", fullpath);
        item->has_origin = (mod_ctx->ctx->origin_path != NULL);
        if (item->has_origin) {
            strncpy (item->origin_fullpath, mod_ctx->ctx->origin_path, PATH_MAX - 1);
            concat_str (item->origin_fullpath, upath, "/", PATH_MAX);
        }
        item->offset = offset;
        item->length = length;
        item->send_detached_is_thread_safe = mod_ctx->ctx->dtl_handle->send_detached_is_thread_safe;
        item->mod_ctx = mod_ctx;
        // Standard Flux idiom: retain a reference to the request message
        // past this synchronous callback -- needed regardless of backend
        // for the trailing flux_respond_error() call in
        // dyad_fetch_completion_watcher_cb().
        item->msg = (flux_msg_t *)flux_msg_incref (msg);

        rc = mod_ctx->ctx->dtl_handle->rpc_detach_request (mod_ctx->ctx, &item->req_state);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: rpc_detach_request failed");
            flux_msg_decref (item->msg);
            free (item);
            errno = EPROTO;
            goto fetch_range_error_wo_flock;
        }
        request_detached = true;
        // Sized to the client-requested length as an upper bound -- the
        // true clamped range_len isn't known until the worker opens the
        // file, functionally identical to the fallback path below, just
        // computed earlier.
        rc = mod_ctx->ctx->dtl_handle->get_buffer (mod_ctx->ctx, length, &item->inbuf);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx,
                            "DYAD_MOD: Could not allocate DTL buffer for ranged fetch");
            mod_ctx->ctx->dtl_handle->rpc_send_detached (mod_ctx->ctx,
                                                          item->req_state,
                                                          NULL,
                                                          DYAD_DTL_SEND_ERROR);
            flux_msg_decref (item->msg);
            free (item);
            errno = ENOMEM;
            goto fetch_range_error_wo_flock;
        }

        pthread_mutex_lock (&mod_ctx->work_mutex);
        item->next = NULL;
        if (mod_ctx->work_tail == NULL) {
            mod_ctx->work_head = item;
        } else {
            mod_ctx->work_tail->next = item;
        }
        mod_ctx->work_tail = item;
        pthread_cond_signal (&mod_ctx->work_cond);
        pthread_mutex_unlock (&mod_ctx->work_mutex);

        DYAD_C_FUNCTION_END ();
        return;
    }

    // Fallback path (UCX, or if the worker pool failed to start): fully
    // synchronous, unchanged from before threaded servicing existed.
    if (mod_ctx->ctx->origin_path != NULL) {
        char origin_fullpath[PATH_MAX + 1] = {'\0'};
        strncpy (origin_fullpath, mod_ctx->ctx->origin_path, PATH_MAX - 1);
        concat_str (origin_fullpath, upath, "/", PATH_MAX);
        rc = dyad_range_cache_ensure (mod_ctx->ctx, fullpath, origin_fullpath, offset, length);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (mod_ctx->ctx,
                            "DYAD_MOD: dyad_range_cache_ensure failed for file \"%s\".",
                            fullpath);
            errno = EIO;
            goto fetch_range_error_wo_flock;
        }
    }

    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Reading file %s for ranged transfer", fullpath);
    fd = open (fullpath, O_RDONLY);

    if (fd < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Failed to open file \"%s\".", fullpath);
        goto fetch_range_error_wo_flock;
    }
    rc = dyad_shared_flock (mod_ctx->ctx, fd, &shared_lock);
    if (DYAD_IS_ERROR (rc)) {
        goto fetch_range_error;
    }
    file_size = get_file_size (fd);
    // Clamp against the real file size for safety -- offset/length come
    // from the consumer's own index and should already be valid, but a
    // stale/corrupt index must not turn into an out-of-bounds pread().
    if ((ssize_t)offset >= file_size) {
        range_len = 0;
    } else if ((ssize_t)(offset + length) > file_size) {
        range_len = file_size - (ssize_t)offset;
    } else {
        range_len = (ssize_t)length;
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx,
                    "DYAD_MOD: file %s has size %zd, serving range [%zu, %zu)",
                    fullpath,
                    file_size,
                    offset,
                    offset + (size_t)range_len);
    rc = mod_ctx->ctx->dtl_handle->get_buffer (mod_ctx->ctx, (size_t)range_len, (void **)&inbuf);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not allocate DTL buffer for ranged fetch");
        goto fetch_range_error;
    }
    if (range_len > 0l) {
        if (range_len < DYAD_POSIX_TRANSFER_GRANULARITY) {
            inlen = pread (fd, inbuf, (size_t)range_len, (off_t)offset);
        } else {
            ssize_t read_data = 0;
            int granularity = DYAD_POSIX_TRANSFER_GRANULARITY;
            while (read_data < range_len) {
                ssize_t read_size =
                    (range_len - read_data) > granularity ? granularity : (range_len - read_data);
                inlen = pread (fd,
                               inbuf + read_data,
                               (size_t)read_size,
                               (off_t)(offset + (size_t)read_data));
                DYAD_LOG_DEBUG (mod_ctx->ctx,
                                "DYAD_MOD: reading range of file %s with bytes %zd of %zd",
                                fullpath,
                                read_size,
                                inlen);
                if (inlen <= 0) {
                    DYAD_LOG_ERROR (mod_ctx->ctx,
                                    "DYAD_MOD: Failed to load range of file \"%s\" only read %zd "
                                    "of %zd of %zd. with code %d:%s.",
                                    fullpath,
                                    inlen,
                                    read_size,
                                    range_len,
                                    errno,
                                    strerror (errno));
                    goto fetch_range_error;
                }
                read_data += inlen;
            }
            inlen = read_data;
        }
        if (inlen != range_len) {
            DYAD_LOG_ERROR (mod_ctx->ctx,
                            "DYAD_MOD: Failed to load range of file \"%s\" only read %zd of %zd. "
                            "with code %d:%s.",
                            fullpath,
                            inlen,
                            range_len,
                            errno,
                            strerror (errno));
            goto fetch_range_error;
        }
    }
    DYAD_C_FUNCTION_UPDATE_INT ("range_len", range_len);
    dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
    close (fd);
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Establish DTL connection with consumer");
    rc = mod_ctx->ctx->dtl_handle->establish_connection (mod_ctx->ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not establish DTL connection with client");
        errno = ECONNREFUSED;
        goto fetch_range_error_wo_flock;
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Send file range to consumer with DTL");
    rc = mod_ctx->ctx->dtl_handle->send (mod_ctx->ctx, inbuf, (size_t)range_len);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: Could not send range data to client via DTL\n");
        errno = ECOMM;
        goto fetch_range_error_wo_flock;
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx, "DYAD_MOD: Close DTL connection with consumer");
    mod_ctx->ctx->dtl_handle->close_connection (mod_ctx->ctx);
    mod_ctx->ctx->dtl_handle->return_buffer (mod_ctx->ctx, (void **)&inbuf);

    DYAD_LOG_DEBUG (mod_ctx->ctx,
                    "DYAD_MOD: Close RPC message stream with an ENODATA (%d) message",
                    ENODATA);
    if (flux_respond_error (h, msg, ENODATA, NULL) < 0) {
        DYAD_LOG_DEBUG (mod_ctx->ctx,
                        "DYAD_MOD: %s: flux_respond_error with ENODATA failed\n",
                        __func__);
    }
    DYAD_LOG_DEBUG (mod_ctx->ctx,
                    "DYAD_MOD: Finished %s module invocation\n",
                    DYAD_DTL_RPC_RANGE_NAME);
    goto end_fetch_range_cb;

fetch_range_error:;
    dyad_release_flock (mod_ctx->ctx, fd, &shared_lock);
    close (fd);

fetch_range_error_wo_flock:;
    if (request_unpacked && !request_detached && mod_ctx->ctx->dtl_handle->mode != DYAD_DTL_FLUX_RPC
        && !DYAD_IS_ERROR (mod_ctx->ctx->dtl_handle->establish_connection (mod_ctx->ctx))) {
        mod_ctx->ctx->dtl_handle->send (mod_ctx->ctx, NULL, DYAD_DTL_SEND_ERROR);
        mod_ctx->ctx->dtl_handle->close_connection (mod_ctx->ctx);
    }
    DYAD_LOG_ERROR (mod_ctx->ctx,
                    "DYAD_MOD: Close RPC message stream with an error (errno = %d)\n",
                    errno);
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: %s: flux_respond_error", __func__);
    }
    errno = saved_errno;
    DYAD_C_FUNCTION_END ();
    return;

end_fetch_range_cb:;
    errno = saved_errno;
    DYAD_C_FUNCTION_END ();
    return;
}

/**
 * @brief Flux message handler table for the DYAD module.
 *
 * @details
 * Registers @c dyad_fetch_request_cb for whole-file @c DYAD_DTL_RPC_NAME
 * ("dyad.fetch") requests and @c dyad_fetch_range_request_cb for byte-range
 * @c DYAD_DTL_RPC_RANGE_NAME ("dyad.fetch_range") requests. Consumers send
 * fetch requests to whichever topic matches the API they called
 * (@c dyad_consume()/@c dyad_consume_w_metadata() vs @c dyad_consume_range()),
 * and the reactor dispatches them to the corresponding handler.
 *
 * Passed to @c flux_msg_handler_addvec() in @c mod_main() and terminated
 * by @c FLUX_MSGHANDLER_TABLE_END as required by the Flux API.
 */
static const struct flux_msg_handler_spec htab[] =
    {{FLUX_MSGTYPE_REQUEST, DYAD_DTL_RPC_NAME, dyad_fetch_request_cb, 0},
     {FLUX_MSGTYPE_REQUEST, DYAD_DTL_RPC_RANGE_NAME, dyad_fetch_range_request_cb, 0},
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
    DYAD_LOG_STDOUT (
        "    -o, --origin_path: Fallback source path (e.g. on the parallel\n"
        "                       file system) used to lazily fill missing\n"
        "                       spans of a managed file on demand. Need a\n"
        "                       path as an argument. Omit to require files\n"
        "                       be fully staged upfront (default).\n");
}

/**
 * @brief Parsed command-line options for the DYAD Flux module.
 */
struct opt_parse_out {
    const char *prod_managed_path;  ///< Producer-managed directory path, or @c NULL.
    const char *dtl_mode;           ///< DTL mode string, or @c NULL for default.
    const char *origin_path;        ///< Fallback origin path, or @c NULL to disable.
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
 *  - @c -o / @c --origin_path Sets @c opt->origin_path (lazy origin-backed
 *                             range cache fallback source).
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
                                           {"origin_path", required_argument, 0, 'o'},
                                           {0, 0, 0, 0}};

    int c;
    while ((c = getopt_long (_argc, _argv, "hdm:i:e:o:", long_options, NULL)) != -1) {
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
            case 'o':
                DYAD_LOG_STDERR ("DYAD_MOD: 'origin_path' option -o with value `%s'\n", optarg);
                opt->origin_path = optarg;
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

    if (opt->origin_path) {
        setenv (DYAD_PATH_ORIGIN_ENV, opt->origin_path, 1);
        DYAD_LOG_STDOUT ("DYAD_MOD: origin_path option set. Setting env %s=%s\n",
                         DYAD_PATH_ORIGIN_ENV,
                         opt->origin_path);
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

    opt_parse_out_t opt = {NULL, NULL, NULL, false, false};

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

    // Not fatal if it fails -- dyad_fetch_range_request_cb() checks
    // num_fetch_workers and falls back to the fully-synchronous path, so a
    // pool-start failure degrades performance but not correctness.
    if (DYAD_IS_ERROR (dyad_fetch_pool_start (mod_ctx))) {
        DYAD_LOG_STDERR (
            "DYAD_MOD: fetch worker pool failed to start; falling back to "
            "synchronous fetch servicing\n");
    }

    if (flux_reactor_run (flux_get_reactor (mod_ctx->ctx->h), 0) < 0) {
        DYAD_LOG_ERROR (mod_ctx->ctx, "DYAD_MOD: flux_reactor_run: %s\n", strerror (errno));
        dyad_fetch_pool_stop (mod_ctx);
        goto mod_error;
    }
    dyad_fetch_pool_stop (mod_ctx);
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
