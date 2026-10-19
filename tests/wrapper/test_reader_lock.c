/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <dyad/client/dyad_client.h>
#include <dyad/core/dyad_ctx.h>
#include <dyad/utils/utils.h>

/**
 * @brief Checks a parent-held reader lock from a separate process.
 *
 * @details
 * The child opens through @c syscall so its descriptor does not re-enter the
 * preloaded DYAD @c open wrapper. It then attempts a nonblocking whole-file
 * exclusive POSIX lock.
 *
 * @param[in] path            Existing file to test.
 * @param[in] expect_blocked  True when the exclusive lock must conflict;
 *                            false when it must succeed.
 * @return @c true when the child observed the requested lock state; otherwise
 *         @c false.
 */
static bool check_exclusive_lock (const char *path, bool expect_blocked)
{
    const pid_t pid = fork ();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        const int fd = (int)syscall (SYS_openat, AT_FDCWD, path, O_RDWR);
        if (fd < 0) {
            _exit (EXIT_FAILURE);
        }
        struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
        const int rc = fcntl (fd, F_SETLK, &lock);
        const bool blocked = rc < 0 && (errno == EACCES || errno == EAGAIN);
        if (rc == 0) {
            lock.l_type = F_UNLCK;
            (void)fcntl (fd, F_SETLK, &lock);
        }
        (void)close (fd);
        _exit (blocked == expect_blocked ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    int status = 0;
    return waitpid (pid, &status, 0) == pid && WIFEXITED (status)
           && WEXITSTATUS (status) == EXIT_SUCCESS;
}

/**
 * @brief Checks whether cache eviction can lock a candidate file.
 *
 * @param[in] ctx             Initialized DYAD context.
 * @param[in] path            Existing file to test.
 * @param[in] expect_blocked  True when the cache lock must conflict.
 * @return @c true when the cache lock has the requested outcome; otherwise
 *         @c false.
 */
static bool check_cache_lock (dyad_ctx_t *ctx, const char *path, bool expect_blocked)
{
    const int fd = (int)syscall (SYS_openat, AT_FDCWD, path, O_RDWR);
    if (fd < 0) {
        return false;
    }
    struct flock lock;
    const dyad_rc_t rc = dyad_cache_try_excl_flock (ctx, fd, &lock);
    if (rc == DYAD_RC_OK) {
        (void)dyad_cache_release_flock (ctx, fd, &lock);
    }
    (void)syscall (SYS_close, fd);
    return (rc == DYAD_RC_BUSY) == expect_blocked;
}

/**
 * @brief Verifies the consume guard protects the reader-lock handoff.
 *
 * @param[in] path  Existing DYAD-managed file to consume and open.
 * @return @c true when cache eviction is blocked by the guard and then by the
 *         application reader lock, and proceeds after reader close; otherwise
 *         @c false.
 */
static bool test_guard_handoff (const char *path)
{
    dyad_ctx_t *ctx = dyad_ctx_get ();
    bool is_managed = false;
    int guard_fd = -1;
    if (ctx == NULL || DYAD_IS_ERROR (dyad_consume (ctx, path, &is_managed, &guard_fd))
        || !is_managed || guard_fd < 0 || !check_cache_lock (ctx, path, true)) {
        if (guard_fd >= 0) {
            (void)syscall (SYS_close, guard_fd);
        }
        return false;
    }

    const int reader_fd = (int)syscall (SYS_openat, AT_FDCWD, path, O_RDONLY);
    if (reader_fd < 0) {
        (void)syscall (SYS_close, guard_fd);
        return false;
    }
    struct flock reader_lock;
    if (DYAD_IS_ERROR (dyad_ofd_shared_flock (ctx, reader_fd, &reader_lock))) {
        (void)syscall (SYS_close, reader_fd);
        (void)syscall (SYS_close, guard_fd);
        return false;
    }
    (void)syscall (SYS_close, guard_fd);
    if (!check_cache_lock (ctx, path, true)) {
        (void)dyad_ofd_release_flock (ctx, reader_fd, &reader_lock);
        (void)syscall (SYS_close, reader_fd);
        return false;
    }
    (void)dyad_ofd_release_flock (ctx, reader_fd, &reader_lock);
    (void)syscall (SYS_close, reader_fd);
    return check_cache_lock (ctx, path, false);
}

/**
 * @brief Verifies a failed intercepted open releases its transferred guard.
 *
 * @param[in] path  Existing managed regular file to open as a directory.
 * @return @c true when @c open() fails with @c ENOTDIR and cache eviction can
 *         subsequently lock the file; otherwise @c false.
 */
static bool test_failed_open_releases_guard (const char *path)
{
    dyad_ctx_t *ctx = dyad_ctx_get ();
    errno = 0;
    const int fd = open (path, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        (void)close (fd);
        return false;
    }
    return ctx != NULL && errno == ENOTDIR && check_cache_lock (ctx, path, false);
}

/**
 * @brief Tests reader locks acquired by intercepted @c open() and @c fopen().
 *
 * @param[in] path  Existing DYAD-managed file to read.
 * @return @c true when both read APIs block a separate process's exclusive
 *         lock until their descriptor is closed; otherwise @c false.
 */
static bool test_reader_locks (const char *path)
{
    int fd = open (path, O_RDONLY);
    if (fd < 0 || !check_exclusive_lock (path, true)) {
        if (fd >= 0) {
            (void)close (fd);
        }
        return false;
    }
    (void)close (fd);
    if (!check_exclusive_lock (path, false)) {
        return false;
    }

    FILE *stream = fopen (path, "r");
    if (stream == NULL || !check_exclusive_lock (path, true)) {
        if (stream != NULL) {
            (void)fclose (stream);
        }
        return false;
    }
    (void)fclose (stream);
    return check_exclusive_lock (path, false);
}

/**
 * @brief Runs the C interception-wrapper reader-lock regression.
 *
 * @param[in] argc  Number of command-line arguments.
 * @param[in] argv  Program name and the existing managed file path.
 * @return @c EXIT_SUCCESS when all lock checks pass; @c EXIT_FAILURE for bad
 *         arguments or a failed check.
 */
int main (int argc, char **argv)
{
    return argc == 2 && test_guard_handoff (argv[1]) && test_failed_open_releases_guard (argv[1])
                   && test_reader_locks (argv[1])
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
