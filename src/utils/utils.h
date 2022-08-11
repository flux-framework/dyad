/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_UTILS_H
#define DYAD_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#define DYAD_PATH_DELIM "/"

#if defined(__cplusplus)
//#include <cstdbool> // c++11
#include <cstddef>
#else
#include <stdbool.h>
#include <stddef.h>
#endif // defined(__cplusplus)

#include <sys/stat.h>

#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

bool file_in_read_mode(FILE *f);
bool fd_in_read_mode(int fd);
bool oflag_is_read(int oflag);
bool mode_is_read(const char *mode);

void enable_debug_dyad_utils (void);
void disable_debug_dyad_utils (void);
bool check_debug_dyad_utils (void);

char* concat_str (char* __restrict__ str,
                  const char* __restrict__ to_append,
                  const char* __restrict__ connector,
                  size_t str_capacity);

bool extract_user_path (const char* __restrict__ path,
                        char* __restrict__ upath,
                        const size_t upath_len);

bool cmp_prefix (const char*  __restrict__ prefix,
                 const char*  __restrict__ full,
                 const char*  __restrict__ delim,
                 size_t*  __restrict__ u_len);

bool cmp_canonical_path_prefix (const char* __restrict__ prefix,
                                const char* __restrict__ path,
                                char* __restrict__ upath,
                                const size_t upath_capacity);

int mkdir_as_needed (const char* path, const mode_t m);

/// Obtain path from the file descriptor
int get_path (const int fd, const size_t max_size, char* path);

/// Check if the path is a directory
bool is_path_dir (const char* path);

/// Check if the file identified by the file descriptor is a directory
bool is_fd_dir (int fd);

#if DYAD_SPIN_WAIT
/// Try access file info
bool get_stat (const char* path, unsigned int max_retry, long ns_sleep);
#endif // DYAD_SPIN_WAIT

#if DYAD_SYNC_DIR
/**
 * Run fsync for the containing directory of the given path.
 * For example, if path is "/a/b", then fsync on "/a".
 * This cannot be used with DYAD interception.
 */
int sync_containing_dir (const char* path);
#endif // DYAD_SYNC_DIR

#if defined(__cplusplus)
};
#endif // defined(__cplusplus)

#endif // DYAD_UTILS_H
