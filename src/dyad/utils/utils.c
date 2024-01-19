/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  // _GNU_SOURCE
#include <dyad/utils/utils.h>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>

#include <fcntl.h>      // open
#include <libgen.h>     // basename dirname
#include <sys/stat.h>   // open
#include <sys/types.h>  // open
#include <unistd.h>     // readlink

#if defined(__cplusplus)
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
// #include <cstdbool> // c++11
#else
#include <errno.h>
#include <limits.h>  // PATH_MAX
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif

static __thread bool debug_dyad_utils = false;

void enable_debug_dyad_utils (void)
{
    debug_dyad_utils = true;
}

void disable_debug_dyad_utils (void)
{
    debug_dyad_utils = false;
}

bool check_debug_dyad_utils (void)
{
    return debug_dyad_utils;
}

/**
 * Append the string `to_append` to the existing string `str`.
 * A connector is added between them. `str_capacity` indicates
 * how large the `str` buffer is.
 */
char* concat_str (char* __restrict__ str,
                  const char* __restrict__ to_append,
                  const char* __restrict__ connector,
                  size_t str_capacity)
{
    const size_t str_len_org = strlen (str);
    const size_t con_len = strlen (connector);
    const size_t all_len = str_len_org + con_len + strlen (to_append);

    if (all_len + 1ul > str_capacity) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffer is too small.\n");
        return NULL;
    }

    bool con_end = false;
    if ((connector != NULL) && (str_len_org >= con_len)) {
        con_end = (strncmp (str + str_len_org - con_len, connector, con_len) == 0);
    }
    const size_t str_len = (con_end ? (str_len_org - con_len) : str_len_org);

    const char* const str_end = str + str_capacity;
    bool no_overlap =
        ((to_append + strlen (to_append) <= str) || (str_end <= to_append))
        && ((connector + strlen (connector) <= str) || (str_end <= connector));

    if (!no_overlap) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.\n");
        return NULL;
    }

    char* buf = (char*)calloc (all_len + 1ul, sizeof (char));

    strncpy (buf, str, str_len);

    if (connector != NULL) {
        strncat (buf, connector, all_len);
    }
    strncat (buf, to_append, all_len);

    strncpy (str, buf, all_len);
    str[all_len] = '\0';
    free (buf);

    return str;
}

/**
 * Copies the last upath_len characters of the path into upath
 */
bool extract_user_path (const char* __restrict__ path,
                        char* __restrict__ upath,
                        const size_t upath_len)
{
    if ((upath_len > PATH_MAX) || (upath == NULL)) {
        return false;
    }

    if (!(((upath + PATH_MAX) <= path) || ((path + PATH_MAX) <= upath))) {
        return false;  // buffers overlap
    }

    size_t upath_pos = strlen (path) - upath_len;
    strncpy (upath, path + upath_pos, upath_len);
    upath[upath_len] = '\0';

    return true;
}

/**
 * Compares a path (full) against a path prefix considering the path
 * delimiter. Then, returns the length of the user added path string
 * that follows the path prefix via the last argument.
 */
bool cmp_prefix (const char* __restrict__ prefix,
                 const char* __restrict__ full,
                 const char* __restrict__ delim,
                 size_t* __restrict__ u_len)
{
    {
        const char* const u_len_end = ((const char*)u_len) + sizeof (size_t);
        bool no_overlap =
            ((prefix + strlen (prefix) <= (char*)u_len) || (u_len_end <= prefix))
            && ((full + strlen (full) <= (char*)u_len) || (u_len_end <= full))
            && ((delim + strlen (delim) <= (char*)u_len) || (u_len_end <= delim));

        if (!no_overlap) {
            DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.\n");
            return false;
        }
    }

    const size_t full_len = strlen (full);
    const size_t delim_len = ((delim == NULL) ? 0ul : strlen (delim));
    size_t prefix_len = strlen (prefix);
    if (full_len > PATH_MAX || delim_len > PATH_MAX || prefix_len > PATH_MAX) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: path length cannot be larger than PATH_MAX\n");
        return false;
    }

    if (delim_len == 0ul) {  // no delimiter is defined
        if (strncmp (prefix, full, prefix_len) != 0) {
            return false;
        }
        if (u_len != NULL) {
            // Without a path delimiter used, the length of the substring that
            // follows the prefix (user path) is the total length of the full
            // string minus that of the prefix string.
            *u_len = full_len - prefix_len;
        }
        return true;
    }

    if (prefix_len >= delim_len) {
        if (strncmp (prefix + prefix_len - delim_len, delim, delim_len) == 0) {
            // disregard the delimiter at the end of the prefix string
            prefix_len -= delim_len;
        }
    }

    // Check if the full path begins with the prefix string
    if (strncmp (prefix, full, prefix_len) != 0) {
        return false;
    }

    if (full_len == prefix_len) {
        // The full path string is exactly the same as the prefix string.
        if (u_len != NULL) {
            *u_len = 0ul;
        }
        return true;
    } else if (full_len < prefix_len + delim_len) {
        return false;
    }

    if (u_len != NULL) {
        *u_len = full_len - prefix_len - delim_len;
    }

    // Finally, make sure that the user path begins with the delimiter.
    // This check correctly identifies, for example, "prefix_path2/user_path" as
    // a mismatch when "prefix_path/user_path" is a match.
    return (strncmp (full + prefix_len, delim, delim_len) == 0);
}

bool cmp_canonical_path_prefix (const char* __restrict__ prefix,
                                const char* __restrict__ path,
                                char* __restrict__ upath,
                                const size_t upath_capacity)
{
    {
        const char* const upath_end = upath + upath_capacity;
        bool no_overlap = ((prefix + strlen (prefix) <= upath) || (upath_end <= prefix))
                          && ((path + strlen (path) <= upath) || (upath_end <= path));

        if (!no_overlap) {
            DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap\n");
            return false;
        }
    }

    // Only works when there are no multiple absolute paths via hardlinks
    char can_prefix[PATH_MAX] = {'\0'};  // canonical form of the managed path
    char can_path[PATH_MAX] = {'\0'};    // canonical form of the given path

    // The path prefix needs to translate to a real path
    if (!realpath (prefix, can_prefix)) {
        DYAD_LOG_DEBUG (NULL,"DYAD UTIL: error in realpath for %s\n", prefix);
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: %s\n", strerror (errno));
        return false;
    }

    size_t upath_len = 0ul;
    bool match = false;

    // See if the prefix of the path in question matches that of either the
    // dyad managed path or its canonical form when the path is not a real one.
    if (!realpath (path, can_path)) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: %s is NOT a realpath.\n", path);
        if (!cmp_prefix (prefix, path, DYAD_PATH_DELIM, &upath_len)) {
            match = cmp_prefix (can_prefix, path, DYAD_PATH_DELIM, &upath_len);
        } else {
            match = true;
        }
        extract_user_path (path, upath, upath_len);
        return match;
    }

    // For a real path, see if the prefix of either the path or its canonical
    // form matches the managed path.
    match = cmp_prefix (can_prefix, can_path, DYAD_PATH_DELIM, &upath_len);
    if (upath_len + 1 > upath_capacity) {
        return false;
    }
    extract_user_path (can_path, upath, upath_len);

    return match;
}

/**
 * Recursively create a directory
 * https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix
 */
int mkpath (const char* dir, const mode_t m)
{
    struct stat sb;

    if (!dir) {
        errno = EINVAL;
        return 1;
    }

    if (!stat (dir, &sb))
        return 0;

    mkpath (dirname (strdupa (dir)), m);

    return mkdir (dir, m);
}

int mkdir_as_needed (const char* path, const mode_t m)
{
    if (path == NULL || strlen (path) == 0ul) {
        DYAD_LOG_DEBUG (NULL, "Cannot create a directory with no name\n");
        return -3;
    }

    struct stat sb;

    const mode_t RWX_UGO = (S_IRWXU | S_IRWXG | S_IRWXO);

    if (stat (path, &sb) == 0) {          // already exist
        if (S_ISDIR (sb.st_mode) == 0) {  // not a directory
            DYAD_LOG_DEBUG (NULL, "\"%s\" already exists not as a directory\n", path);
            return -2;
        } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
            DYAD_LOG_DEBUG (NULL,
                "Directory \"%s\" already exists with "
                "different permission bits %o from "
                "the requested %o\n",
                path,
                (sb.st_mode & RWX_UGO),
                (m & RWX_UGO));
            return 5;  // already exists but with different mode
        }
        return 1;  // already exists
    }
    DYAD_LOG_DEBUG (NULL, "Creating directory \"%s\"\n", path);

    const mode_t old_mask = umask (0);
    if (mkpath (path, m) != 0) {
        if (stat (path, &sb) == 0) {          // already exist
            if (S_ISDIR (sb.st_mode) == 0) {  // not a directory
                DYAD_LOG_DEBUG (NULL,"\"%s\" already exists not as a directory\n", path);
                return -4;
            } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
                DYAD_LOG_DEBUG (NULL,
                    "Directory \"%s\" already exists with "
                    "different permission bits %o from "
                    "the requested %o\n",
                    path,
                    (sb.st_mode & RWX_UGO),
                    (m & RWX_UGO));
                return 5;  // already exists but with different mode
            }
            return 1;  // already exists
        }
        DYAD_LOG_DEBUG (NULL, "Cannot create directory \"%s\": %s\n", path, strerror (errno));
        perror ("mkdir_as_needed() ");
        return -1;
    }
    umask (old_mask);

#if DYAD_SYNC_DIR
    sync_containing_dir (path);
#endif  // DYAD_SYNC_DIR

    return 0;  // The new directory has been succesfully created
}

int get_path (const int fd, const size_t max_size, char* path)
{
    if (max_size < 1ul) {
        DYAD_LOG_DEBUG (NULL, "Invalid max path size.\n");
        return -1;
    }
    memset (path, '\0', max_size + 1);
    char proclink[PATH_MAX] = {'\0'};

    sprintf (proclink, "/proc/self/fd/%d", fd);
    ssize_t rc = readlink (proclink, path, max_size);
    if (rc < (ssize_t)0) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: error reading the file link (%s): %s\n",
                 strerror (errno),
                 proclink);
        return -1;
    } else if ((size_t)rc == max_size) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: truncation might have happend with %s\n", proclink);
    }
    path[max_size + 1] = '\0';

    return 0;
}

bool is_path_dir (const char* path)
{
    if (path == NULL || strlen (path) == 0ul) {
        return false;
    }

    struct stat sb;

    if ((stat (path, &sb) == 0) && (S_ISDIR (sb.st_mode) != 0)) {
        return true;
    }
    return false;
}

bool is_fd_dir (int fd)
{
    if (fd < 0) {
        return false;
    }

    struct stat sb;

    if ((fstat (fd, &sb) == 0) && (S_ISDIR (sb.st_mode) != 0)) {
        return true;
    }
    return false;
}

#if DYAD_SPIN_WAIT
bool get_stat (const char* path, unsigned int max_retry, long ns_sleep)
{
    struct stat sb;
    unsigned int num_retry = 0u;
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = ns_sleep;

    while ((stat (path, &sb) != 0) && (num_retry < max_retry)) {
        nanosleep (&tim, &tim2);
        num_retry++;
    }
    if (num_retry == max_retry) {
        return false;
    }
    return true;
}
#endif  // DYAD_SPIN_WAIT

ssize_t get_file_size (int fd)
{
    const ssize_t file_size = lseek (fd, 0, SEEK_END);
    if (file_size == 0) {
        errno = EINVAL;
        return 0;
    }
    const off_t offset = lseek (fd, 0, SEEK_SET);
    if (offset != 0) {
        errno = EINVAL;
        return 0;
    }
    return file_size;
}

dyad_rc_t dyad_excl_flock (const dyad_ctx_t* ctx, int fd, struct flock* lock)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Applies an exclusive lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto excl_flock_end;
    }
    lock->l_type = F_WRLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid; //getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) { // will wait until able to lock
        DYAD_LOG_ERROR (ctx, "Cannot apply exclusive lock on fd %d", fd);
        rc = DYAD_RC_BADFIO;
        goto excl_flock_end;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Exclusive lock placed on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    rc = DYAD_RC_OK;
excl_flock_end:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_shared_flock (const dyad_ctx_t* ctx, int fd, struct flock* lock)
{
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Applies a shared lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto shared_flock_end;
    }
    lock->l_type = F_RDLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid; //getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) { // will wait until able to lock
        DYAD_LOG_ERROR (ctx, "Cannot apply shared lock on fd %d", fd);
        rc = DYAD_RC_BADFIO;
        goto shared_flock_end;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Shared lock placed on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    rc = DYAD_RC_OK;
shared_flock_end:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_release_flock (const dyad_ctx_t* ctx, int fd, struct flock* lock)
{
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Releases a lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    lock->l_type = F_UNLCK;
    if (fcntl (fd, F_SETLKW, lock) == -1) { // will just unlock
        DYAD_LOG_ERROR (ctx, "Cannot release lock on fd %d", fd);
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] lock lifted from fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    rc = DYAD_RC_OK;
release_flock_end:;
    DYAD_C_FUNCTION_END();
    return rc;
}

#if DYAD_SYNC_DIR
int sync_containing_dir (const char* path)
{
    char fullpath[PATH_MAX + 2] = {'\0'};
    strncpy (fullpath, path, PATH_MAX);
    const char* containing_dir = dirname (fullpath);
    int dir_fd = open (containing_dir, O_RDONLY);
    if (strlen (containing_dir) == 0) {
        return 0;
    } else if ((strlen (containing_dir) == 2) && (containing_dir[0] == '.')
               && (containing_dir[1] == '/')) {
        return 0;
    }

    if (dir_fd < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg,
                  PATH_MAX + 256,
                  "Failed to open directory %s\n",
                  containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    if (fsync (dir_fd) < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg, PATH_MAX + 256, "Failed to fsync %s\n", containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    if (close (dir_fd) < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg, PATH_MAX + 256, "Failed to close %s\n", containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    return 0;
}

#endif  // DYAD_SYNC_DIR
