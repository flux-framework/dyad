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
#include <libgen.h>  // dirname
#include <unistd.h>

#include "utils.h"
// #include "wrapper.h"
#include "dyad_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static __thread dyad_ctx_t *ctx = NULL;
static void dyad_wrapper_init (void) __attribute__ ((constructor));
static void dyad_wrapper_fini (void) __attribute__ ((destructor));

#if DYAD_SYNC_DIR
int sync_directory (const char *path);
#endif  // DYAD_SYNC_DIR

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

void dyad_wrapper_init (void)
{
    dyad_rc_t rc = DYAD_RC_OK;

    rc = dyad_init_env (&ctx);

    if (DYAD_IS_ERROR (rc)) {
        fprintf (stderr, "Failed to initialize DYAD (code = %d)\n", rc);
        ctx->initialized = false;
        ctx->reenter = false;
        return;
    }

    DYAD_LOG_INFO (ctx, "DYAD Initialized\n");
    DYAD_LOG_INFO (ctx, "%s=%s\n", DYAD_SYNC_DEBUG_ENV, (ctx->debug) ? "true" : "false");
    DYAD_LOG_INFO (ctx, "%s=%s\n", DYAD_SYNC_CHECK_ENV, (ctx->check) ? "true" : "false");
    DYAD_LOG_INFO (ctx, "%s=%u\n", DYAD_KEY_DEPTH_ENV, ctx->key_depth);
    DYAD_LOG_INFO (ctx, "%s=%u\n", DYAD_KEY_BINS_ENV, ctx->key_bins);
}

void dyad_wrapper_fini ()
{
    if (ctx == NULL) {
        return;
    }
    dyad_finalize (&ctx);
}

DYAD_DLL_EXPORTED int open (const char *path, int oflag, ...)
{
    char *error = NULL;
    typedef int (*open_ptr_t) (const char *, int, mode_t, ...);
    open_ptr_t func_ptr = NULL;
    int mode = 0;

    if (oflag & O_CREAT) {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }

    func_ptr = (open_ptr_t)dlsym (RTLD_NEXT, "open");
    if ((error = dlerror ())) {
        DPRINTF (ctx, "DYAD_SYNC: error in dlsym: %s\n", error);
        return -1;
    }

    if ((mode != O_RDONLY) || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter)) {
        IPRINTF (ctx, "DYAD_SYNC: open sync not applicable for \"%s\".\n", path);
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters open sync (\"%s\").\n", path);
    if (DYAD_IS_ERROR (dyad_consume (ctx, path))) {
        DPRINTF (ctx, "DYAD_SYNC: failed open sync (\"%s\").\n", path);
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exists open sync (\"%s\").\n", path);

real_call:;

    return (func_ptr (path, oflag, mode));
}

DYAD_DLL_EXPORTED FILE *fopen (const char *path, const char *mode)
{
    char *error = NULL;
    typedef FILE *(*fopen_ptr_t) (const char *, const char *);
    fopen_ptr_t func_ptr = NULL;

    func_ptr = (fopen_ptr_t)dlsym (RTLD_NEXT, "fopen");
    if ((error = dlerror ())) {
        DPRINTF (ctx, "DYAD_SYNC: error in dlsym: %s\n", error);
        return NULL;
    }

    if ((strcmp (mode, "r") != 0) || is_path_dir (path)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter) || !path) {
        IPRINTF (ctx,
                 "DYAD_SYNC: fopen sync not applicable for \"%s\".\n",
                 ((path) ? path : ""));
        goto real_call;
    }

    IPRINTF (ctx, "DYAD_SYNC: enters fopen sync (\"%s\").\n", path);
    if (DYAD_IS_ERROR (dyad_consume (ctx, path))) {
        DPRINTF (ctx, "DYAD_SYNC: failed fopen sync (\"%s\").\n", path);
        goto real_call;
    }
    IPRINTF (ctx, "DYAD_SYNC: exits fopen sync (\"%s\").\n", path);

real_call:
    return (func_ptr (path, mode));
}

DYAD_DLL_EXPORTED int close (int fd)
{
    bool to_sync = false;
    char *error = NULL;
    typedef int (*close_ptr_t) (int);
    close_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;

    func_ptr = (close_ptr_t)dlsym (RTLD_NEXT, "close");
    if ((error = dlerror ())) {
        DPRINTF (ctx, "DYAD_SYNC: error in dlsym: %s\n", error);
        return -1;  // return the failure code
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
        IPRINTF (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;  // semicolon here to avoid the error
    // "a label can only be part of a statement and a declaration is not a
    // statement"
    fsync (fd);

#if DYAD_SYNC_DIR
    if (to_sync) {
        dyad_sync_directory (ctx, path);
    }
#endif  // DYAD_SYNC_DIR

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx,
                 "Failed to check the mode of the file with fcntl: %s\n",
                 strerror (errno));
    }

    if (to_sync && wronly == 1) {
        rc = func_ptr (fd);
        if (rc != 0) {
            DPRINTF (ctx, "Failed close (\"%s\").: %s\n", path, strerror (errno));
        }
        IPRINTF (ctx, "DYAD_SYNC: enters close sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed close sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits close sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fd);
    }

    return rc;
}

DYAD_DLL_EXPORTED int fclose (FILE *fp)
{
    bool to_sync = false;
    char *error = NULL;
    typedef int (*fclose_ptr_t) (FILE *);
    fclose_ptr_t func_ptr = NULL;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;
    int fd = 0;

    func_ptr = (fclose_ptr_t)dlsym (RTLD_NEXT, "fclose");
    if ((error = dlerror ())) {
        DPRINTF (ctx, "DYAD_SYNC: error in dlsym: %s\n", error);
        return EOF;  // return the failure code
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
        IPRINTF (ctx, "DYAD_SYNC: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;
    fflush (fp);
    fd = fileno (fp);
    fsync (fd);

#if DYAD_SYNC_DIR
    if (to_sync) {
        dyad_sync_directory (ctx, path);
    }
#endif  // DYAD_SYNC_DIR

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DPRINTF (ctx,
                 "Failed to check the mode of the file with fcntl: %s\n",
                 strerror (errno));
    }

    if (to_sync && wronly == 1) {
        rc = func_ptr (fp);
        if (rc != 0) {
            DPRINTF (ctx, "Failed fclose (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: enters fclose sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx, path))) {
            DPRINTF (ctx, "DYAD_SYNC: failed fclose sync (\"%s\").\n", path);
        }
        IPRINTF (ctx, "DYAD_SYNC: exits fclose sync (\"%s\").\n", path);
    } else {
        rc = func_ptr (fp);
    }

    return rc;
}

#ifdef __cplusplus
}
#endif

/*
 * vi: ts=4 sw=4 expandtab
 */
