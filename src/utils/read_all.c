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
#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "read_all.h"

void * allocate_aligned(size_t size) {
  size_t alignment = getpagesize();
  int malloc_offset = (size + alignment) % alignment; 
  void* mem = malloc(size + alignment);
  return (char*) mem + malloc_offset;
}
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

ssize_t read_all (const char* filename, void **bufp)
{
    int fd = open (filename, O_RDONLY);
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
    const ssize_t file_size = lseek (fd, 0, SEEK_END);
    if (file_size == 0) {
        errno = EINVAL;
        return 0;
    }
    off_t offset = lseek (fd, 0, SEEK_SET);
    if (offset != 0) {
        errno = EINVAL;
        return -1;
    }
    *bufp = allocate_aligned (file_size);
    if (*bufp == NULL) {
        errno = EINVAL;
        return -1;
    }
    if ((*bufp = mmap (*bufp, file_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0)) == (caddr_t) -1) {
        errno = EINVAL;
        return -1;
    }
    return file_size;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
