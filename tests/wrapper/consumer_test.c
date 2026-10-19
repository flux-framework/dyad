/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../../common/libtap/tap.h"
#include "../../common/utils.h"
#include "../../core/dyad_envs.h"

int dyad_sync_health ()
{
    char *e = NULL;
    if ((e = getenv (DYAD_SYNC_HEALTH_ENV))) {
        if (!strcmp ("ok", e))
            return 0;
    }
    return -1;
}

/**
 * @brief Checks whether a separate process can take an exclusive file lock.
 *
 * @details
 * The child uses @c openat through @c syscall so this check observes the lock
 * held by the parent without re-entering the DYAD @c open wrapper.  The child
 * exits successfully only when its nonblocking exclusive lock has the result
 * specified by @p expect_blocked.
 *
 * @param[in] path            Existing file to test.
 * @param[in] expect_blocked  True when the child must observe a conflicting
 *                            shared reader lock; false when it must acquire
 *                            and release the exclusive lock.
 * @return @c 0 if the child observed the expected lock state; otherwise
 *         @c -1.
 */
static int check_exclusive_lock (const char *path, bool expect_blocked)
{
    const pid_t pid = fork ();
    if (pid < 0) {
        return -1;
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
    return (waitpid (pid, &status, 0) == pid && WIFEXITED (status)
            && WEXITSTATUS (status) == EXIT_SUCCESS)
               ? 0
               : -1;
}

void test_open_valid_path (const char *dyad_path)
{
    int fd = -1;

    setenv (DYAD_SYNC_HEALTH_ENV, "test", 1);

    char path[PATH_MAX + 1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    strncat (path, "data1.txt", PATH_MAX);
    fd = open (path, O_RDONLY);

    ok (!(fd < 0) && (dyad_sync_health () == 0), "consumer: open sync");
    ok (check_exclusive_lock (path, true) == 0,
        "consumer: open holds a shared lock against a separate process");

    setenv (DYAD_SYNC_HEALTH_ENV, "test", 1);
    close (fd);
    ok ((dyad_sync_health () == 0), "consumer: close sync");
    ok (check_exclusive_lock (path, false) == 0, "consumer: close releases the shared lock");
}

void test_fopen_valid_path (const char *dyad_path)
{
    int rc = -1;
    FILE *fptr = NULL;

    setenv (DYAD_SYNC_HEALTH_ENV, "test", 1);

    char path[PATH_MAX + 1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    strncat (path, "data2.txt", PATH_MAX);
    fptr = fopen (path, "r");

    if (fptr == NULL) {
        fprintf (stderr, "Cannot open file %s: %s\n", path, strerror (errno));
    }

    ok ((fptr != NULL) && (dyad_sync_health () == 0), "consumer: fopen sync");
    ok (check_exclusive_lock (path, true) == 0,
        "consumer: fopen holds a shared lock against a separate process");

    setenv (DYAD_SYNC_HEALTH_ENV, "test", 1);
    rc = fclose (fptr);
    ok ((rc == 0) && (dyad_sync_health () == 0), "consumer: fclose sync");
    ok (check_exclusive_lock (path, false) == 0, "consumer: fclose releases the shared lock");
}

int main (int argc, char *argv[])
{
    plan (8);

    const char *dyad_path = ".";
    if (argc > 1) {
        dyad_path = argv[1];
    }

    char path[PATH_MAX + 1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    size_t dyad_path_len = strlen (dyad_path);

    if (dyad_path_len == 0ul) {
        fprintf (stderr, "DYAD_PATH_CONSUMER cannot be defined.\n");
        return EXIT_FAILURE;
    }

    if (dyad_path[dyad_path_len - 1] != '/') {
        path[dyad_path_len] = '/';
        path[dyad_path_len + 1] = '\0';
    }

    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if (mkdir_as_needed (path, m) < 0) {
        fprintf (stderr, "Directory %s cannot be created.\n", path);
    }

    setenv (DYAD_PATH_CONSUMER_ENV, path, 1);
    setenv (DYAD_SYNC_CHECK_ENV, "1", 1);

    test_open_valid_path (path);

    test_fopen_valid_path (path);

    done_testing ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */
