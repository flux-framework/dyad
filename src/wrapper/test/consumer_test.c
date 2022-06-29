/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../common/libtap/tap.h"
#include "../../common/utils.h"

int dyad_sync_health ()
{
    char *e = NULL;
    if ((e = getenv ("DYAD_SYNC_HEALTH"))) {
        if (!strcmp ("ok", e))
            return 0;
    }
    return -1;
}

void test_open_valid_path (const char *dyad_path)
{
    int fd = -1;

    setenv ("DYAD_SYNC_HEALTH", "test", 1);

    char path[PATH_MAX+1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    strncat (path, "data1.txt", PATH_MAX);
    fd = open (path, O_RDONLY);

    ok (!(fd < 0) && (dyad_sync_health () == 0), "consumer: open sync");

    setenv ("DYAD_SYNC_HEALTH", "test", 1);
    close (fd);
    ok ((dyad_sync_health () == 0), "consumer: close sync");
}

void test_fopen_valid_path (const char *dyad_path)
{
    int rc = -1;
    FILE *fptr = NULL;

    setenv ("DYAD_SYNC_HEALTH", "test", 1);

    char path[PATH_MAX+1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    strncat (path, "data2.txt", PATH_MAX);
    fptr = fopen (path, "r");

    if (fptr == NULL) {
       fprintf (stderr, "Cannot open file %s: %s\n", path, strerror (errno));
    }

    ok ((fptr != NULL) && (dyad_sync_health () == 0), "consumer: fopen sync");

    setenv ("DYAD_SYNC_HEALTH", "test", 1);
    rc = fclose (fptr);
    ok ((rc == 0) && (dyad_sync_health () == 0), "consumer: fclose sync");
}

int main (int argc, char *argv[])
{
    plan (4);

    const char* dyad_path = ".";
    if (argc > 1) {
        dyad_path = argv[1];
    }

    char path[PATH_MAX+1] = {'\0'};
    strncpy (path, dyad_path, PATH_MAX);
    size_t dyad_path_len = strlen (dyad_path);

    if (dyad_path_len == 0ul) {
        fprintf (stderr, "DYAD_PATH_CONSUMER cannot be defined.\n");
        return EXIT_FAILURE;
    }

    if (dyad_path[dyad_path_len - 1] != '/') {
        path[dyad_path_len] = '/';
        path[dyad_path_len+1] = '\0';
    }

    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if (mkdir_as_needed (path, m) < 0) {
        fprintf (stderr, "Directory %s cannot be created.\n", path);
    }

    setenv ("DYAD_KIND_CONSUMER", "1", 1);
    setenv ("DYAD_PATH_CONSUMER", path, 1);
    setenv ("DYAD_SYNC_CHECK", "1", 1);

    test_open_valid_path (path);

    test_fopen_valid_path (path);

    done_testing ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */
