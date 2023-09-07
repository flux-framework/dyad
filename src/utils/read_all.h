/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_UTILS_READ_ALL_H
#define DYAD_UTILS_READ_ALL_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif  // defined(__cplusplus)

ssize_t write_all (int fd, const void *buf, size_t len);

#if DYAD_PERFFLOW
__attribute__ ((annotate ("@critical_path()")))
#endif
ssize_t
read_all (int fd, void **bufp);

#if defined(__cplusplus)
};
#endif  // defined(__cplusplus)

#endif /* DYAD_UTILS_READ_ALL_H */
