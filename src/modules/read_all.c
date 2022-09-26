/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "read_all.h"

ssize_t write_all (int fd, const void *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t count = 0;
    char* buf_pos = NULL;

    if (fd < 0 || (buf == NULL && len != 0)) {
        errno = EINVAL;
        return -1;
    }
    while ((size_t) count < len) {
        buf_pos = (char*) buf + count;
        if ((n = write (fd, (void*) buf_pos, len - count)) < 0)
            return -1;
        count += n;
    }
    return count;
}

ssize_t read_all (int fd, void **bufp)
{
    const size_t chunksize = 4096;
    size_t len = 0;
    void *buf = NULL;
    ssize_t n = 0;
    ssize_t count = 0;
    void *new_buf = NULL;
    int saved_errno = errno;
    char* buf_pos = NULL;

    if (fd < 0 || !bufp) {
        errno = EINVAL;
        return -1;
    }
    do {
        if (len - count == 0) {
            len += chunksize;
            if (!(new_buf = realloc (buf, len+1)))
                goto error;
            buf = new_buf;
        }
        buf_pos = (char*) buf + count;
        if ((n = read (fd, (void*) buf_pos, len - count)) < 0)
            goto error;
        count += n;
    } while (n != 0);
    ((char*)buf)[count] = '\0';
    *bufp = buf;
    return count;
error:
    saved_errno = errno;
    free (buf);
    errno = saved_errno;
    return -1;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
