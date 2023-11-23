/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include "io_c.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>  // dirname ()
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../common/utils.h"  // mkdir_as_needed ()

#if 0
#include <time.h>
#define PRINT_CLOCK(Who, What)                                        \
    do {                                                              \
        struct timespec ts;                                           \
        clock_gettime (CLOCK_MONOTONIC_RAW, &ts);                     \
        char buff[100];                                               \
        strftime (buff, sizeof (buff), "%D %T", gmtime (&ts.tv_sec)); \
        printf (Who " " What " at %s.%09ld UTC\n", buff, ts.tv_nsec); \
    } while (0)
#else
#define PRINT_CLOCK(Who, What)
#endif

int mkdir_of_path (const char* path)
{
    char fullpath[PATH_MAX + 2] = {'\0'};
    strncpy (fullpath, path, PATH_MAX);
    const char* upper_dir = dirname (fullpath);
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if (mkdir_as_needed (upper_dir, m) < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg, PATH_MAX + 256, "Directory %s cannot be created.\n", upper_dir);
        perror (errmsg);
        return -1;
    }
    return 0;
}

static void fill_buf (char* buffer, size_t buf_sz, const char* line)
{
    memset (buffer, 0, buf_sz);
    if (buf_sz < 2ul) {
        if (buf_sz == 1ul) {
            buffer[0] = '\0';
        }
        return;
    }

    // Reserve the space at the end of the buffer at least for two characters.
    // One is for terminating null charactor at the end of the buffer.
    // Another is for a newline character with which we will replace the null
    // placed by the sprintf ().
    const size_t e_buf_sz = buf_sz - 1ul;  // effective buffer size
    char* bp = buffer;
    const char* bp_end = bp + e_buf_sz;
    unsigned int i = 0u;

    while (bp + 1u < bp_end) {
        snprintf (bp, bp_end - bp, "%5u\t%s\n", i++, line);
        bp += strlen (bp);
    }

    // set the last character in the buffer as the terminating null character
    buffer[e_buf_sz] = '\0';
    if (e_buf_sz == 1ul) {
        buffer[0] = '\n';
    }

    // The sprintf set the last character in the effective
    if (e_buf_sz >= 2ul) {
        if (buffer[e_buf_sz - 2] != '\n') {
            buffer[e_buf_sz - 1] = '\n';
        } else {
            buffer[e_buf_sz - 1] = '\n';
            buffer[e_buf_sz - 2] = ' ';
        }
    }
}

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static int
produce_fd (int ofd, size_t sz)
{
    size_t b = 0ul;
    size_t n = 0ul;

    char buffer[PATH_MAX + 1] = {'\0'};
    const size_t buf_sz = PATH_MAX;
    const size_t fill_sz = ((buf_sz < sz) ? buf_sz : sz);
    fill_buf (buffer, fill_sz + 1ul, "test produce_fd");

    for (; n + fill_sz < sz; n += fill_sz) {
        b = write (ofd, buffer, fill_sz);
        if (b == 0ul)
            return -1;
    }
    if (n < sz) {
        b = write (ofd, buffer, sz - n);
        if (b == 0ul)
            return -1;
    }

    return 0;
}

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static int
produce_fp (FILE* fp, size_t sz)
{
    size_t b = 0;
    size_t n = 0ul;

    char buffer[PATH_MAX + 1] = {'\0'};
    const size_t buf_sz = PATH_MAX;
    const size_t fill_sz = ((buf_sz < sz) ? buf_sz : sz);
    fill_buf (buffer, fill_sz + 1ul, "test produce_fp");

    for (; n + fill_sz < sz; n += fill_sz) {
        b = fprintf (fp, "%s", buffer);
        if (b == 0ul)
            return -1;
    }
    if (n < sz) {
        b = fprintf (fp, "%s", buffer);
        if (b == 0ul)
            return -1;
    }

    return 0;
}

int test_prod_io (const char* pf, const size_t sz)
{
    int fd = -1;
    int rc = 0;

    // Create upper directory of the file if it does not exist yet.
    // mkdir_of_path (pf);

    // Open the file to write
    fd = open (pf, O_WRONLY | O_CREAT | O_SYNC, 0666);
    if (fd < 0) {
        fprintf (stderr, "PROD: Cannot open %s : %s\n", pf, strerror (errno));
        return -1;
    }

    // Write some contents into the file as much as `sz` bytes
    rc = produce_fd (fd, sz);
    if (rc != 0) {
        fprintf (stderr, "PROD: Cannot write %s\n", pf);
        return -1;
    }

#if USER_FSYNC
    // Sychronize with the underlying storge
    fsync (fd);
#endif  // USER_FSYNC

    // Close the file
    rc = close (fd);
    PRINT_CLOCK ("Prod", "closes");
    if (rc != 0) {
        fprintf (stderr, "PROD: Cannot close %s\n", pf);
        return -1;
    }
    return 0;
}

int test_prod_io_buf (const char* pf, const size_t sz)
{
    int rc = -1;
    FILE* fptr = NULL;

    // Create upper directory of the file if it does not exist yet.
    // mkdir_of_path (pf);

    // Open the file to write
    fptr = fopen (pf, "a");
    if (fptr == NULL) {
        fprintf (stderr, "PROD: Cannot fopen %s : %s\n", pf, strerror (errno));
        return -1;
    }

    // Write some contents into the file as much as `sz` bytes
    rc = produce_fp (fptr, sz);
    if (rc != 0) {
        fprintf (stderr, "PROD: Cannot fprintf %s\n", pf);
        return -1;
    }

#if USER_FSYNC
    // Sychronize with the underlying storge
    fflush (fptr);
    fsync (fileno (fptr));
#endif  // USER_FSYNC

    // Close the file
    rc = fclose (fptr);
    PRINT_CLOCK ("Prod", "closes");
    if (rc != 0) {
        fprintf (stderr, "PROD: Cannot fclose %s\n", pf);
        return -1;
    }
    return 0;
}

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static int
check_read (const char* buf, size_t sz, const char* line)
{
    char buffer[PATH_MAX + 1] = {'\0'};
    const size_t buf_sz = PATH_MAX;
    const size_t fill_sz = ((buf_sz < sz) ? buf_sz : sz) + 1ul;
    fill_buf (buffer, fill_sz, line);

    size_t i = 0ul;
    size_t j = 0ul;
    for (; i < sz; ++i) {
        if (buffer[j] != buf[i]) {
            fprintf (stderr,
                     "error at %lu th character: %c != %c \n",
                     i,
                     buffer[j],
                     buf[i]);
            return -2;
        }
        if (++j == buf_sz)
            j = 0ul;
    }

    return 0;
}

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static int
consume_fd (int ifd, size_t sz, int verify)
{
    char* buf = (char*)malloc (sz + 1);
    if (buf == NULL)
        return -1;

    memset (buf, 0, sz + 1);
    ssize_t n = read (ifd, buf, sz);

    if (((ssize_t)sz) != n) {
        free (buf);
        return -1;
    }
    int rc = ((verify != 0) ? check_read (buf, sz, "test produce_fd") : 0);

    free (buf);
    return rc;
}

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
static int
consume_fp (FILE* ifp, size_t sz, int verify)
{
    char* buf = (char*)malloc (sz + 1);
    if (buf == NULL)
        return -1;

    memset (buf, 0, sz + 1);
    size_t n = fread (buf, 1, sz, ifp);

    if (n != sz) {
        free (buf);
        fprintf (stderr, " read %lu bytes instead of %lu\n", n, sz);
        return -1;
    }
    int rc = ((verify != 0) ? check_read (buf, sz, "test produce_fp") : 0);

    free (buf);
    return rc;
}

int test_cons_io (const char* pf, size_t sz, int verify)
{
    int fd = -1;
    int rc = 0;

    // Create upper directory of the file if it does not exist yet.
    // mkdir_of_path (pf);

    fd = open (pf, O_RDONLY);
    PRINT_CLOCK ("Cons", "opens");
    if (fd < 0) {
        fprintf (stderr, "CONS: Cannot open %s\n", pf);
        return -1;
    }

    // Read the file contents into memory buffer as much as `sz` bytes
    rc = consume_fd (fd, sz, verify);
    if (rc == -2) {
        fprintf (stderr, "CONS: Wrong bytes read from %s\n", pf);
        close (fd);
        return -1;
    } else if (rc == -1) {
        fprintf (stderr, "CONS: Cannot read %s\n", pf);
        close (fd);
        return -1;
    }

    // Close the file
    rc = close (fd);
    if (rc != 0) {
        fprintf (stderr, "CONS: Cannot close %s\n", pf);
        return -1;
    }
    return 0;
}

int test_cons_io_buf (const char* pf, size_t sz, int verify)
{
    int rc = -1;
    FILE* fptr = NULL;

    // Create upper directory of the file if it does not exist yet.
    // mkdir_of_path (pf);

    fptr = fopen (pf, "r");
    PRINT_CLOCK ("Cons", "opens");
    if (fptr == NULL) {
        fprintf (stderr, "CONS: Cannot fopen %s\n", pf);
        return -1;
    }

    // Read the file contents into memory buffer as much as `sz` bytes
    rc = consume_fp (fptr, sz, verify);
    if (rc == -2) {
        fprintf (stderr, "CONS: Wrong bytes fread from %s\n", pf);
        fclose (fptr);
        return -1;
    } else if (rc == -1) {
        fprintf (stderr, "CONS: Cannot fread %s\n", pf);
        fclose (fptr);
        return -1;
    }

    // Close the file
    rc = fclose (fptr);
    if (rc != 0) {
        fprintf (stderr, "CONS: Cannot fclose %s\n", pf);
        return -1;
    }
    return 0;
}
