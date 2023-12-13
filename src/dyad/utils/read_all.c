/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "read_all.h"

ssize_t write_all (int fd, const void *buf, size_t len)
{
    ssize_t n = 0;
    ssize_t count = 0;
    char *buf_pos = NULL;

    if (fd < 0 || (buf == NULL && len != 0)) {
        errno = EINVAL;
        return -1;
    }
    while ((size_t)count < len) {
        buf_pos = (char *)buf + count;
        if ((n = write (fd, (void *)buf_pos, len - count)) < 0)
            return -1;
        count += n;
    }
    return count;
}

ssize_t get_file_size (int fd)
{
    ssize_t file_size = 0;
    off_t offset = 0;
    file_size = lseek (fd, 0, SEEK_END);
    if (file_size == 0) {
        errno = EINVAL;
        return -1;
    }
    offset = lseek (fd, 0, SEEK_SET);
    if (offset != 0) {
        errno = EINVAL;
        return -1;
    }
    return file_size;
}

ssize_t read_all (int fd, void *bufp, ssize_t file_size)
{
    if (bufp == NULL) {
        errno = EINVAL;
        return -1;
    }
    ssize_t bytes_read = read (fd, bufp, file_size);
    if (bytes_read < file_size) {
        // could not read all data
        errno = EINVAL;
        return bytes_read;
    }
    return file_size;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
