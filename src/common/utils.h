/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef DYAD_UTILS_H
#define DYAD_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

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
