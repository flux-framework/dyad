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

#include <dyad/client/dyad_client_int.h>
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>
#include <libgen.h>  // dirname
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#include <gotcha/gotcha.h>
#pragma clang diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

static __thread const dyad_ctx_t *ctx = NULL;
static __thread dyad_ctx_t *ctx_mutable = NULL;
static void dyad_wrapper_init (void) __attribute__ ((constructor));
static void dyad_wrapper_fini (void) __attribute__ ((destructor));

#if DYAD_SYNC_DIR
int sync_directory (const char *path);
#endif  // DYAD_SYNC_DIR

/* Forward declarations of wrapper functions, required for the GOTCHA binding table. */
static int dyad_open_wrapper (const char *path, int oflag, ...);
static FILE *dyad_fopen_wrapper (const char *path, const char *mode);
static int dyad_close_wrapper (int fd);
static int dyad_fclose_wrapper (FILE *fp);
static int dyad_open64_wrapper (const char *path, int oflag, ...);
static FILE *dyad_fopen64_wrapper (const char *path, const char *mode);
static int dyad_close64_wrapper (int fd);
static int dyad_fclose64_wrapper (FILE *fp);

/* GOTCHA wrappee handles -- one per intercepted symbol.
 * After gotcha_wrap(), each handle holds the address of the real function. */
static gotcha_wrappee_handle_t wrappee_open_handle;
static gotcha_wrappee_handle_t wrappee_fopen_handle;
static gotcha_wrappee_handle_t wrappee_close_handle;
static gotcha_wrappee_handle_t wrappee_fclose_handle;
static gotcha_wrappee_handle_t wrappee_open64_handle;
static gotcha_wrappee_handle_t wrappee_fopen64_handle;
static gotcha_wrappee_handle_t wrappee_close64_handle;
static gotcha_wrappee_handle_t wrappee_fclose64_handle;

/* GOTCHA binding table: { symbol_name, wrapper_fn, &wrappee_handle } */
static struct gotcha_binding_t dyad_bindings[] = {
    {"open", __extension__ (void *) dyad_open_wrapper, &wrappee_open_handle},
    {"fopen", __extension__ (void *) dyad_fopen_wrapper, &wrappee_fopen_handle},
    {"close", __extension__ (void *) dyad_close_wrapper, &wrappee_close_handle},
    {"fclose", __extension__ (void *) dyad_fclose_wrapper, &wrappee_fclose_handle},
    {"open64", __extension__ (void *) dyad_open64_wrapper, &wrappee_open64_handle},
    {"fopen64", __extension__ (void *) dyad_fopen64_wrapper, &wrappee_fopen64_handle},
    {"close64", __extension__ (void *) dyad_close64_wrapper, &wrappee_close64_handle},
    {"fclose64", __extension__ (void *) dyad_fclose64_wrapper, &wrappee_fclose64_handle},
};

/*****************************************************************************
 *                                                                           *
 *                   DYAD Sync Internal API                                  *
 *                                                                           *
 *****************************************************************************/

/**
 * Checks if the file descriptor was opened in write-only mode
 *
 * @param[in] fd The file descriptor to check
 *
 * @return 1 if the file descriptor is write-only, 0 if not, and -1
 *         if there was an error in fcntl
 */
static inline int is_wronly (int fd)
{
    int rc = fcntl (fd, F_GETFL);
    if (rc == -1)
        return -1;
    if ((rc & O_ACCMODE) == O_WRONLY)
        return 1;
    return 0;
}

/*****************************************************************************
 *                                                                           *
 *         DYAD Sync Constructor, Destructor and Wrapper API                 *
 *                                                                           *
 *****************************************************************************/

/**
 * @brief Initializes the DYAD GOTCHA wrapper.
 *
 * @details
 * Initializes the DYAD context via @c dyad_ctx_init(), which internally
 * calls @c dyad_init_env() to read configuration from environment variables,
 * which in turn calls @c dyad_init() to set up the full DYAD context.
 * After initialization, registers the GOTCHA bindings in @c dyad_bindings
 * and optionally sets the GOTCHA interception priority from the
 * @c DYAD_GOTCHA_PRIORITY environment variable. Called automatically at
 * library load time via a constructor attribute.
 *
 * Sets @c ctx->use_fs_locks to @c true since the C GOTCHA wrapper always
 * has direct access to file descriptors and filesystem locking is always
 * available. See @c dyad_consume() for where this flag is checked.
 *
 * @todo If the C GOTCHA wrapper and the C++ stream wrapper ever co-exist
 *       in the same process, @c use_fs_locks should become context-dependent
 *       rather than a single global value, since the C++ stream wrapper path
 *       requires @c DYAD_HAS_STD_FSTREAM_FD to be defined for filesystem
 *       locking to be available.
 */
static __attribute__ ((constructor)) void dyad_wrapper_init (void)
{
    dyad_ctx_init (DYAD_COMM_RECV, NULL);
    DYAD_C_FUNCTION_START ();  // this is after initialization of profiler
    ctx = ctx_mutable = dyad_ctx_get ();
    // See dyad_consume () in dyad_client.c
    // TODO: In case that the wrapper and c++ stream wrapper class co-exist
    // this variable should be context dependent.
    ctx_mutable->use_fs_locks = true;

    gotcha_wrap (dyad_bindings,
                 sizeof (dyad_bindings) / sizeof (struct gotcha_binding_t),
                 "dyad_wrapper");
    char *gotcha_prio = getenv (DYAD_GOTCHA_PRIORITY_ENV);
    if (gotcha_prio) {
        gotcha_set_priority ("dyad_wrapper", atoi (gotcha_prio));
        DYAD_LOG_DEBUG (ctx, "DYAD Wrapper: Gotcha priority %s", gotcha_prio);
    }

    DYAD_LOG_DEBUG (ctx, "DYAD Wrapper: Initialized");
    DYAD_C_FUNCTION_END ();
}

/**
 * @brief Finalizes the DYAD GOTCHA wrapper.
 *
 * @details
 * Finalizes the DYAD context via @c dyad_ctx_fini(), which internally calls
 * @c dyad_finalize() to flush and release DYAD resources, which in turn
 * calls @c dyad_clear() to free the context and reset it to its initial
 * state. Called automatically at library unload time via a destructor
 * attribute.
 */
static __attribute__ ((destructor)) void dyad_wrapper_fini (void)
{
    DYAD_C_FUNCTION_START ();
    DYAD_LOG_DEBUG (ctx, "DYAD Wrapper: Finalized");
    DYAD_C_FUNCTION_END ();  // this is before teardown of profiler
    dyad_ctx_fini ();
}

/**
 * @brief Closes a short-lived consumer lock guard without publishing a file.
 *
 * @param[in] guard_fd  Transferred guard descriptor, or a negative value.
 */
static void dyad_close_consume_guard (int guard_fd)
{
    if (guard_fd < 0) {
        return;
    }
    const bool reenter = ctx_mutable->reenter;
    ctx_mutable->reenter = false;
    (void)close (guard_fd);
    ctx_mutable->reenter = reenter;
}

/**
 * @brief GOTCHA wrapper for @c open() that integrates DYAD synchronization
 *        and data transfer.
 *
 * @details
 * Intercepts @c open() calls and performs DYAD consumer or producer-side
 * operations before delegating to the real @c open().
 *
 * On the consumer side, if the context is valid and the re-entrancy guard is
 * not active, calls @c dyad_consume() which ensures the file is ready,
 * potentially triggering a data transfer from the producer if the file is
 * not yet locally available. After the real read-only open succeeds, takes a
 * shared OFD lock. Closing the descriptor releases that lock.
 *
 * On the producer side, after a successful @c open() in write or append mode
 * on a file under the producer-managed path, acquires an exclusive lock to
 * prevent consumers, who have direct visibility (e.g. via shared storage or
 * co-location on the same node), from reading partially written file. No data
 * transfer is performed on the producer side at open time; that is handled by
 * @c dyad_close_wrapper() when the file is closed.
 *
 * The following cases bypass synchronization and go directly to the real
 * @c open():
 *  - The file is a directory, or is not opened read-only — consumer sync
 *    is not applicable.
 *  - The DYAD context is invalid or the re-entrancy guard is active,
 *    indicating this call originated from within a DYAD operation and
 *    must not be intercepted again.
 *
 * @param[in] path   Path to the file to open.
 * @param[in] oflag  Open flags passed to @c open(). If @c O_CREAT is set,
 *                   the @p mode argument is extracted from the variadic list.
 * @param[in] ...    Optional @c mode_t permission bits, required when
 *                   @c O_CREAT is set in @p oflag.
 *
 * @return int
 * @retval >=0  File descriptor returned by the real @c open().
 * @retval -1   The GOTCHA wrappee could not be retrieved (@c errno set to
 *              @c ENOSYS), or the real @c open() failed (@c errno set by
 *              @c open()).
 *
 * @note This function is registered with GOTCHA and is not intended to be
 *       called directly.
 */
static int dyad_open_wrapper (const char *path, int oflag, ...)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", "path");
    typedef int (*open_ptr_t) (const char *, int, mode_t, ...);
    open_ptr_t func_ptr = NULL;
    int mode = 0;
    const int access_mode = oflag & O_ACCMODE;
    bool is_managed = false;
    int guard_fd = -1;
    dyad_rc_t consume_rc = DYAD_RC_OK;
    char upath[PATH_MAX + 1] = {'\0'};

    if (oflag & O_CREAT) {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }

    func_ptr = __extension__ (open_ptr_t) gotcha_get_wrappee (wrappee_open_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'open()'");
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    if (access_mode != O_RDONLY || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter)) {
        IPRINTF (ctx, "DYAD_SYNC: open sync not applicable for \"%s\".", path);
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters open sync (\"%s\").", path);
    consume_rc = dyad_consume (ctx_mutable, path, &is_managed, &guard_fd);
    if (DYAD_IS_ERROR (consume_rc)) {
        DPRINTF (ctx, "DYAD_SYNC: failed open sync (\"%s\").", path);
        is_managed = false;
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exits open sync (\"%s\").", path);

real_call:;
    int ret = (func_ptr (path, oflag, mode));

    if (ret >= 0 && is_managed) {
        struct flock shared_lock;
        dyad_rc_t rc = dyad_ofd_shared_flock (ctx, ret, &shared_lock);
        if (DYAD_IS_ERROR (rc)) {
            dyad_ofd_release_flock (ctx, ret, &shared_lock);
        }
    }
    dyad_close_consume_guard (guard_fd);

    // This lock is to prevent consumers that has direct access to the file
    // from reading the file being produced by a producer. For example,
    // either the file is on a shared storage or the consumer is on
    // the same node as where the producer is.
    if (ret > 0 && access_mode != O_RDONLY && !is_path_dir (path)) {
        if ((ctx->relative_to_managed_path
             && (strncmp (path, DYAD_PATH_DELIM, ctx->delim_len) != 0))
            || cmp_canonical_path_prefix (ctx, true, path, upath, PATH_MAX)) {
            struct flock exclusive_lock;
            dyad_rc_t rc = dyad_excl_flock (ctx, ret, &exclusive_lock);
            if (DYAD_IS_ERROR (rc)) {
                dyad_release_flock (ctx, ret, &exclusive_lock);
            }
        }
    }

    DYAD_C_FUNCTION_END ();
    return ret;
}

/**
 * @brief GOTCHA wrapper for @c fopen() that integrates DYAD synchronization.
 *
 * @details
 * Functionally equivalent to @c dyad_open_wrapper() but intercepts
 * @c fopen() instead of @c open(). A successful managed @c "r" open takes a
 * shared OFD lock, which @c fclose() releases with the descriptor.
 *
 * @see dyad_open_wrapper()
 */
static FILE *dyad_fopen_wrapper (const char *path, const char *mode)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", "path");
    typedef FILE *(*fopen_ptr_t) (const char *, const char *);
    fopen_ptr_t func_ptr = NULL;
    bool is_managed = false;
    int guard_fd = -1;
    dyad_rc_t consume_rc = DYAD_RC_OK;
    char upath[PATH_MAX + 1] = {'\0'};

    func_ptr = __extension__ (fopen_ptr_t) gotcha_get_wrappee (wrappee_fopen_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'fopen()'");
        DYAD_C_FUNCTION_END ();
        return NULL;
    }

    if ((strcmp (mode, "r") != 0) || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter) || !path) {
        IPRINTF (ctx, "DYAD_SYNC: fopen sync not applicable for \"%s\".\n", ((path) ? path : ""));
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters fopen sync (\"%s\").\n", path);
    consume_rc = dyad_consume (ctx_mutable, path, &is_managed, &guard_fd);
    if (DYAD_IS_ERROR (consume_rc)) {
        DPRINTF (ctx, "DYAD_SYNC: failed fopen sync (\"%s\").\n", path);
        is_managed = false;
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exits fopen sync (\"%s\").\n", path);

real_call:;
    FILE *fh = (func_ptr (path, mode));

    if (fh != NULL && is_managed) {
        const int fd = fileno (fh);
        struct flock shared_lock;
        dyad_rc_t rc = dyad_ofd_shared_flock (ctx, fd, &shared_lock);
        if (DYAD_IS_ERROR (rc)) {
            dyad_ofd_release_flock (ctx, fd, &shared_lock);
        }
    }
    dyad_close_consume_guard (guard_fd);

    // This lock is to prevent consumers that has direct access to the file
    // from reading the file being produced by a producer. For example,
    // either the file is on a shared storage or the consumer is on
    // the same node as where the producer is.
    if ((fh != NULL) && ((strcmp (mode, "w") == 0) || (strcmp (mode, "a") == 0))
        && !is_path_dir (path)) {
        if ((ctx->relative_to_managed_path
             && (strncmp (path, DYAD_PATH_DELIM, ctx->delim_len) != 0))
            || cmp_canonical_path_prefix (ctx, true, path, upath, PATH_MAX)) {
            int fd = fileno (fh);
            struct flock exclusive_lock;
            dyad_rc_t rc = dyad_excl_flock (ctx, fd, &exclusive_lock);
            if (DYAD_IS_ERROR (rc)) {
                dyad_release_flock (ctx, fd, &exclusive_lock);
            }
        }
    }
    DYAD_C_FUNCTION_END ();
    return fh;
}

/**
 * @brief GOTCHA wrapper for @c close() that integrates DYAD producer-side
 *        synchronization.
 *
 * @details
 * Intercepts @c close() calls to notify consumers that a file is ready via
 * @c dyad_produce(). Unlike @c dyad_open_wrapper(), which may trigger data
 * transfer on the consumer side, this wrapper is purely about signaling —
 * it publishes the file's metadata to the Flux KVS so that waiting consumers
 * can proceed, but does not perform any data transfer itself.
 *
 * If the file descriptor is valid, the context is valid, the re-entrancy
 * guard is not active, the descriptor does not refer to a directory, and the
 * file was opened for writing, the wrapper:
 *  1. Optionally calls @c fsync() and @c dyad_sync_directory() if
 *     @c ctx->fsync_write is enabled.
 *  2. Releases the exclusive lock acquired during @c dyad_open_wrapper().
 *  3. Calls the real @c close().
 *  4. Calls @c dyad_produce() to publish the file metadata to the Flux KVS
 *
 * If any of the preconditions are not met, or the file was not opened for
 * writing, the real @c close() is called directly without synchronization.
 *
 * @param[in] fd  File descriptor to close.
 *
 * @return int
 * @retval  0  The file was successfully closed.
 * @retval -1  The GOTCHA wrappee could not be retrieved (@c errno set to
 *             @c ENOSYS), or the real @c close() failed (@c errno set by
 *             @c close()).
 *
 * @note This function is registered with GOTCHA and is not intended to be
 *       called directly.
 */
static int dyad_close_wrapper (int fd)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    bool to_sync = false;
    typedef int (*close_ptr_t) (int);
    close_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;

    func_ptr = __extension__ (close_ptr_t) gotcha_get_wrappee (wrappee_close_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'close()'");
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    if ((fd < 0) || (ctx == NULL) || (ctx->h == NULL) || !ctx->reenter) {
#if defined(IPRINTF_DEFINED)
        if (ctx == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no context)\n");
        } else if (ctx->h == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no flux)\n");
        } else if (!ctx->reenter) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no reenter)\n");
        } else if (fd >= 0) {
            IPRINTF (ctx,
                     "DYAD_SYNC: close sync not applicable. (invalid file "
                     "descriptor)\n");
        }
#endif  // defined(IPRINTF_DEFINED)
        to_sync = false;
        goto real_call;
    }

    if (is_fd_dir (fd)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (get_path (fd, PATH_MAX - 1, path) < 0) {
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;  // semicolon here to avoid the error
    // "a label can only be part of a statement and a declaration is not a
    // statement"

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx, "Failed to check the mode of the file with fcntl: %s\n", strerror (errno));
    }

    if (to_sync && wronly == 1) {
        if (ctx->fsync_write) {
            fsync (fd);

#if DYAD_SYNC_DIR
            dyad_sync_directory (ctx, path);
#endif  // DYAD_SYNC_DIR
        }

        struct flock exclusive_lock;
        dyad_release_flock (ctx, fd, &exclusive_lock);
        rc = func_ptr (fd);
        if (rc != 0) {
            DPRINTF (ctx, "Failed close (\"%s\").: %s\n", path, strerror (errno));
        }
        IPRINTF (ctx, "DYAD_SYNC: enters close sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx_mutable, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed close sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits close sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fd);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief GOTCHA wrapper for @c fclose() that integrates DYAD synchronization.
 *
 * @details
 * Functionally equivalent to @c dyad_close_wrapper() but intercepts
 * @c fclose() instead of @c close().
 *
 * @see dyad_close_wrapper()
 */
static int dyad_fclose_wrapper (FILE *fp)
{
    DYAD_C_FUNCTION_START ();
    bool to_sync = false;
    typedef int (*fclose_ptr_t) (FILE *);
    fclose_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;
    int fd = 0;

    func_ptr = __extension__ (fclose_ptr_t) gotcha_get_wrappee (wrappee_fclose_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'fclose()'");
        DYAD_C_FUNCTION_END ();
        return EOF;
    }

    if ((fp == NULL) || (ctx == NULL) || (ctx->h == NULL) || !ctx->reenter) {
#if defined(IPRINTF_DEFINED)
        if (ctx == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: fclose sync not applicable. (no context)\n");
        } else if (ctx->h == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: fclose sync not applicable. (no flux)\n");
        } else if (!ctx->reenter) {
            IPRINTF (ctx, "DYAD_SYNC: fclose sync not applicable. (no reenter)\n");
        } else if (fp == NULL) {
            IPRINTF (ctx,
                     "DYAD_SYNC: fclose sync not applicable. (invalid file "
                     "pointer)\n");
        }
#endif  // defined(IPRINTF_DEFINED)
        to_sync = false;
        goto real_call;
    }

    if (is_fd_dir (fileno (fp))) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (get_path (fileno (fp), PATH_MAX - 1, path) < 0) {
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;
    fd = fileno (fp);

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx, "Failed to check the mode of the file with fcntl: %s\n", strerror (errno));
    }

    if (to_sync && wronly == 1) {
        if (ctx->fsync_write) {
            fflush (fp);
            fsync (fd);
#if DYAD_SYNC_DIR
            dyad_sync_directory (ctx, path);
#endif  // DYAD_SYNC_DIR
        }

        struct flock exclusive_lock;
        dyad_release_flock (ctx, fd, &exclusive_lock);
        rc = func_ptr (fp);
        if (rc != 0) {
            DPRINTF (ctx, "Failed fclose (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: enters fclose sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx_mutable, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed fclose sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits fclose sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fp);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief GOTCHA wrapper for @c open64() that integrates DYAD synchronization
 *        and data transfer.
 *
 * @details
 * Functionally equivalent to @c dyad_open_wrapper() but intercepts
 * @c open64() instead of @c open().
 *
 * @see dyad_open_wrapper()
 */
static int dyad_open64_wrapper (const char *path, int oflag, ...)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", "path");
    typedef int (*open64_ptr_t) (const char *, int, mode_t, ...);
    open64_ptr_t func_ptr = NULL;
    int mode = 0;
    const int access_mode = oflag & O_ACCMODE;
    bool is_managed = false;
    int guard_fd = -1;
    dyad_rc_t consume_rc = DYAD_RC_OK;
    char upath[PATH_MAX + 1] = {'\0'};

    if (oflag & O_CREAT) {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }

    func_ptr = __extension__ (open64_ptr_t) gotcha_get_wrappee (wrappee_open64_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'open64()'");
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    if (access_mode != O_RDONLY || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter)) {
        IPRINTF (ctx, "DYAD_SYNC: open64 sync not applicable for \"%s\".", path);
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters open64 sync (\"%s\").", path);
    consume_rc = dyad_consume (ctx_mutable, path, &is_managed, &guard_fd);
    if (DYAD_IS_ERROR (consume_rc)) {
        DPRINTF (ctx, "DYAD_SYNC: failed open64 sync (\"%s\").", path);
        is_managed = false;
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exits open64 sync (\"%s\").", path);

real_call:;
    int ret = (func_ptr (path, oflag, mode));

    if (ret >= 0 && is_managed) {
        struct flock shared_lock;
        dyad_rc_t rc = dyad_ofd_shared_flock (ctx, ret, &shared_lock);
        if (DYAD_IS_ERROR (rc)) {
            dyad_ofd_release_flock (ctx, ret, &shared_lock);
        }
    }
    dyad_close_consume_guard (guard_fd);

    // This lock is to prevent consumers that has direct access to the file
    // from reading the file being produced by a producer. For example,
    // either the file is on a shared storage or the consumer is on
    // the same node as where the producer is.
    if (ret > 0 && access_mode != O_RDONLY && !is_path_dir (path)) {
        if ((ctx->relative_to_managed_path
             && (strncmp (path, DYAD_PATH_DELIM, ctx->delim_len) != 0))
            || cmp_canonical_path_prefix (ctx, true, path, upath, PATH_MAX)) {
            struct flock exclusive_lock;
            dyad_rc_t rc = dyad_excl_flock (ctx, ret, &exclusive_lock);
            if (DYAD_IS_ERROR (rc)) {
                dyad_release_flock (ctx, ret, &exclusive_lock);
            }
        }
    }

    DYAD_C_FUNCTION_END ();
    return ret;
}

/**
 * @brief GOTCHA wrapper for @c fopen64() that integrates DYAD synchronization
 *        and data transfer.
 *
 * @details
 * Functionally equivalent to @c dyad_open_wrapper() but intercepts
 * @c fopen64() instead of @c open().
 *
 * @see dyad_fopen_wrapper()
 */
static FILE *dyad_fopen64_wrapper (const char *path, const char *mode)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", "path");
    typedef FILE *(*fopen64_ptr_t) (const char *, const char *);
    fopen64_ptr_t func_ptr = NULL;
    bool is_managed = false;
    int guard_fd = -1;
    dyad_rc_t consume_rc = DYAD_RC_OK;
    char upath[PATH_MAX + 1] = {'\0'};

    func_ptr = __extension__ (fopen64_ptr_t) gotcha_get_wrappee (wrappee_fopen64_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'fopen64()'");
        DYAD_C_FUNCTION_END ();
        return NULL;
    }

    if ((strcmp (mode, "r") != 0) || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter) || !path) {
        IPRINTF (ctx, "DYAD_SYNC: fopen64 sync not applicable for \"%s\".\n", ((path) ? path : ""));
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters fopen64 sync (\"%s\").\n", path);
    consume_rc = dyad_consume (ctx_mutable, path, &is_managed, &guard_fd);
    if (DYAD_IS_ERROR (consume_rc)) {
        DPRINTF (ctx, "DYAD_SYNC: failed fopen64 sync (\"%s\").\n", path);
        is_managed = false;
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exits fopen64 sync (\"%s\").\n", path);

real_call:;
    FILE *fh = (func_ptr (path, mode));

    if (fh != NULL && is_managed) {
        const int fd = fileno (fh);
        struct flock shared_lock;
        dyad_rc_t rc = dyad_ofd_shared_flock (ctx, fd, &shared_lock);
        if (DYAD_IS_ERROR (rc)) {
            dyad_ofd_release_flock (ctx, fd, &shared_lock);
        }
    }
    dyad_close_consume_guard (guard_fd);

    // This lock is to prevent consumers that has direct access to the file
    // from reading the file being produced by a producer. For example,
    // either the file is on a shared storage or the consumer is on
    // the same node as where the producer is.
    if ((fh != NULL) && ((strcmp (mode, "w") == 0) || (strcmp (mode, "a") == 0))
        && !is_path_dir (path)) {
        if ((ctx->relative_to_managed_path
             && (strncmp (path, DYAD_PATH_DELIM, ctx->delim_len) != 0))
            || cmp_canonical_path_prefix (ctx, true, path, upath, PATH_MAX)) {
            int fd = fileno (fh);
            struct flock exclusive_lock;
            dyad_rc_t rc = dyad_excl_flock (ctx, fd, &exclusive_lock);
            if (DYAD_IS_ERROR (rc)) {
                dyad_release_flock (ctx, fd, &exclusive_lock);
            }
        }
    }
    DYAD_C_FUNCTION_END ();
    return fh;
}

/**
 * @brief GOTCHA wrapper for @c close64() that integrates DYAD synchronization.
 *
 * @details
 * Functionally equivalent to @c dyad_close_wrapper() but intercepts
 * @c close64() instead of @c close().
 *
 * @see dyad_close_wrapper()
 */
static int dyad_close64_wrapper (int fd)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    bool to_sync = false;
    typedef int (*close64_ptr_t) (int);
    close64_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;

    func_ptr = __extension__ (close64_ptr_t) gotcha_get_wrappee (wrappee_close64_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'close64()'");
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    if ((fd < 0) || (ctx == NULL) || (ctx->h == NULL) || !ctx->reenter) {
#if defined(IPRINTF_DEFINED)
        if (ctx == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close64 sync not applicable. (no context)\n");
        } else if (ctx->h == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close64 sync not applicable. (no flux)\n");
        } else if (!ctx->reenter) {
            IPRINTF (ctx, "DYAD_SYNC: close64 sync not applicable. (no reenter)\n");
        } else if (fd >= 0) {
            IPRINTF (ctx,
                     "DYAD_SYNC: close64 sync not applicable. (invalid file "
                     "descriptor)\n");
        }
#endif  // defined(IPRINTF_DEFINED)
        to_sync = false;
        goto real_call;
    }

    if (is_fd_dir (fd)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (get_path (fd, PATH_MAX - 1, path) < 0) {
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;  // semicolon here to avoid the error
    // "a label can only be part of a statement and a declaration is not a
    // statement"

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx, "Failed to check the mode of the file with fcntl: %s\n", strerror (errno));
    }

    if (to_sync && wronly == 1) {
        if (ctx->fsync_write) {
            fsync (fd);

#if DYAD_SYNC_DIR
            dyad_sync_directory (ctx, path);
#endif  // DYAD_SYNC_DIR
        }

        struct flock exclusive_lock;
        dyad_release_flock (ctx, fd, &exclusive_lock);
        rc = func_ptr (fd);
        if (rc != 0) {
            DPRINTF (ctx, "Failed close64 (\"%s\").: %s\n", path, strerror (errno));
        }
        IPRINTF (ctx, "DYAD_SYNC: enters close64 sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx_mutable, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed close64 sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits close64 sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fd);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief GOTCHA wrapper for @c fclose64() that integrates DYAD synchronization.
 *
 * @details
 * Functionally equivalent to @c dyad_close_wrapper() but intercepts
 * @c fclose64() instead of @c close().
 *
 * @note @c fclose64() is not a standard POSIX function unlike the other
 *       64-bit variants (@c open64(), @c fopen64(), @c close64()). It is
 *       provided here for symmetry and platform-specific compatibility.
 *       On most systems @c fclose() already handles large files and
 *       @c fclose64() is simply an alias for it.
 *
 * @see dyad_fclose_wrapper()
 */
static int dyad_fclose64_wrapper (FILE *fp)
{
    DYAD_C_FUNCTION_START ();
    bool to_sync = false;
    typedef int (*fclose64_ptr_t) (FILE *);
    fclose64_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;
    int fd = 0;

    func_ptr = __extension__ (fclose64_ptr_t) gotcha_get_wrappee (wrappee_fclose64_handle);
    if (func_ptr == NULL) {
        errno = ENOSYS;  // return the failure code
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: failed to retrieve gotcha wrapped 'fclose64()'");
        DYAD_C_FUNCTION_END ();
        return EOF;
    }

    if ((fp == NULL) || (ctx == NULL) || (ctx->h == NULL) || !ctx->reenter) {
#if defined(IPRINTF_DEFINED)
        if (ctx == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: fclose64 sync not applicable. (no context)\n");
        } else if (ctx->h == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: fclose64 sync not applicable. (no flux)\n");
        } else if (!ctx->reenter) {
            IPRINTF (ctx, "DYAD_SYNC: fclose64 sync not applicable. (no reenter)\n");
        } else if (fp == NULL) {
            IPRINTF (ctx,
                     "DYAD_SYNC: fclose64 sync not applicable. (invalid file "
                     "pointer)\n");
        }
#endif  // defined(IPRINTF_DEFINED)
        to_sync = false;
        goto real_call;
    }

    if (is_fd_dir (fileno (fp))) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (get_path (fileno (fp), PATH_MAX - 1, path) < 0) {
        DYAD_LOG_DEBUG (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;
    fd = fileno (fp);

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx, "Failed to check the mode of the file with fcntl: %s\n", strerror (errno));
    }

    if (to_sync && wronly == 1) {
        if (ctx->fsync_write) {
            fflush (fp);
            fsync (fd);
#if DYAD_SYNC_DIR
            dyad_sync_directory (ctx, path);
#endif  // DYAD_SYNC_DIR
        }

        struct flock exclusive_lock;
        dyad_release_flock (ctx, fd, &exclusive_lock);
        rc = func_ptr (fp);
        if (rc != 0) {
            DPRINTF (ctx, "Failed fclose64 (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: enters fclose64 sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx_mutable, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed fclose64 sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits fclose64 sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fp);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

#ifdef __cplusplus
}
#endif

/*
 * vi: ts=4 sw=4 expandtab
 */
