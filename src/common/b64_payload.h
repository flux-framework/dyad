/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _UTIL_B64_PAYLOAD_H
#define _UTIL_B64_PAYLOAD_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
ssize_t b64_decode_and_write (int fd, const char *buf, size_t len, size_t chunk_size);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
ssize_t read_and_b64_encode (int fd, char **bufp, size_t chunk_size);

#if defined(__cplusplus)
};
#endif // defined(__cplusplus)

#endif /* !_UTIL_B64_PAYLOAD_H */
