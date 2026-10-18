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
    return argc == 2 && test_reader_locks (argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
