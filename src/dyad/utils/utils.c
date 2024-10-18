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

// logger for utils where it does not depend on flux
#define DYAD_UTIL_LOGGER 1

#include <fcntl.h>   // open
#include <libgen.h>  // basename dirname
#include <sys/file.h>
#include <sys/stat.h>   // open
#include <sys/types.h>  // open
#include <unistd.h>     // readlink

#if defined(__cplusplus)
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif

#include <dyad/utils/utils.h>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/utils/murmur3.h>

#ifndef DYAD_PATH_DELIM
#define DYAD_PATH_DELIM "/"
#endif

/// If hashing is not possible, returns 0. Otherwise, returns a non-zero hash value.
uint32_t hash_str (const char* str, const uint32_t seed)
{
    if (!str)
        return 0u;
    const char* str_long = str;
    size_t str_len = strlen (str);
    if (str_len == 0ul)
        return 0u;

    // Just append the string so that it can be as large as 128 bytes.
    if (str_len < 128ul) {
        char buf[256] = {'\0'};
        memcpy (buf, str, str_len);
        memset (buf + str_len, '@', 128ul - str_len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    uint32_t hash[4] = {0u};  // Output for the hash
    MurmurHash3_x64_128 (str_long, str_len, seed, hash);
    return (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) + 1;
}

/** If hashing is not possible, returns 0. Otherwise, returns a non-zero hash value.
 *  This only hashes the prefix of a given length */
uint32_t hash_path_prefix (const char* str, const uint32_t seed, const size_t len)
{
    if (!str || len == 0ul) {
        return 0u;
    }
    const char* str_long = str;
    size_t str_len = strlen (str);

    if (str_len < len)
        return 0u;
    str_len = len;

    // Just append the string so that it can be as large as 128 bytes.
    if (len < 128ul) {
        char buf[256] = {'\0'};
        memcpy (buf, str, len);
        memset (buf + len, '@', 128ul - len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    uint32_t hash[4] = {0u};  // Output for the hash
    MurmurHash3_x64_128 (str_long, str_len, seed, hash);
    return (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) + 1;
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
    const size_t app_len = strlen (to_append);
    const size_t all_len = str_len_org + con_len + app_len;

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
    bool no_overlap = ((to_append + strlen (to_append) <= str) || (str_end <= to_append))
                      && ((connector + strlen (connector) <= str) || (str_end <= connector));

    if (!no_overlap) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.\n");
        return NULL;
    }

    char* buf = (char*)calloc (all_len + 1ul, sizeof (char));

    memcpy (buf, str, str_len);
    char* buf_pos = buf + str_len;

    if (connector != NULL) {
        memcpy (buf_pos, connector, con_len);
        buf_pos += con_len;
    }
    memcpy (buf_pos, to_append, app_len);

    memcpy (str, buf, all_len);
    str[all_len] = '\0';
    free (buf);

    return str;
}

/**
 * Compares a path (full) against a path prefix considering the path
 * delimiter. Then, returns the length of the user added path string
 * that follows the path prefix via the last argument.
 */
bool extract_user_path (const char* __restrict__ prefix,
                        const char* __restrict__ full,
                        const char* __restrict__ delim,
                        char* __restrict__ upath,
                        const size_t upath_capacity)
{
    size_t prefix_len = strlen (prefix);
    const size_t full_len = strlen (full);
    const size_t delim_len = ((delim == NULL) ? 0ul : strlen (delim));
    size_t u_len = 0ul;

    if (upath == NULL) {
        return false;
    }

    {
        const char* const upath_end = upath + upath_capacity;
        bool no_overlap = ((prefix + prefix_len <= upath) || (upath_end <= prefix))
                          && ((full + full_len <= upath) || (upath_end <= full))
                          && ((delim + delim_len <= upath) || (upath_end <= delim));

        if (!no_overlap) {
            DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.\n");
            return false;
        }
    }

    // Check if the full path begins with the prefix string
    if (strncmp (prefix, full, prefix_len) != 0) {
        return false;
    }

    if (full_len > PATH_MAX || delim_len > PATH_MAX || prefix_len > PATH_MAX) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: path length cannot be larger than PATH_MAX\n");
        return false;
    }

    if (prefix_len >= delim_len) {
        if (strncmp (prefix + prefix_len - delim_len, delim, delim_len) == 0) {
            // disregard the delimiter at the end of the prefix string
            prefix_len -= delim_len;
        }
    }

    if (full_len <= prefix_len + delim_len) {
        // The full path string is exactly the same as the prefix string.
        return false;
    }

    // Finally, make sure that the the delimiter is at the right position.
    // This check correctly identifies, for example, "prefix_path2/user_path" as
    // a mismatch when "prefix_path/user_path" is a match.
    if (strncmp (full + prefix_len, delim, delim_len) != 0) {
        return false;
    }

    u_len = full_len - prefix_len - delim_len;
    if (u_len + 1 > upath_capacity) {
        return false;
    }

    memcpy (upath, full + full_len - u_len, u_len);

    return true;
}

/**
 * This function checks if the 'path' string provided has the prefix that matches
 * the dyad manage path ('prefix') or the canonical version of it ('can_prefix').
 * Then, it returns true if it matches or false otherwise. It also checks with the
 * canonical version of the 'path' if it does not match as is. The portion of the
 * path string without the prefix is written to `upath` buffer. 'upath_capacity'
 * indicates the capacity of the buffer.
 * This does not check if the length of prefix string and can_prefix string
 * actually match the ones provided. It is also assume that the internal hashing
 * relies on the same hash algorithm and the seed as used to compute the hash
 * arguments provided.
 */
bool cmp_canonical_path_prefix (const dyad_ctx_t* __restrict__ ctx,
                                const bool is_prod,
                                const char* __restrict__ path,
                                char* __restrict__ upath,
                                const size_t upath_capacity)
{
    // Only works when there are no multiple absolute paths via hardlinks
    char can_path[PATH_MAX] = {'\0'};  // canonical form of the given path
    if (!ctx) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: Invalid dyad context!\n");
        return false;
    }

    const char* prefix = NULL;
    const char* can_prefix = NULL;
    uint32_t prefix_len = 0u;
    uint32_t can_prefix_len = 0u;
    uint32_t prefix_hash = 0u;
    uint32_t can_prefix_hash = 0u;
    if (is_prod) {
        prefix = ctx->prod_managed_path;
        can_prefix = ctx->prod_real_path;
        prefix_len = ctx->prod_managed_len;
        can_prefix_len = ctx->prod_real_len;
        prefix_hash = ctx->prod_managed_hash;
        can_prefix_hash = ctx->prod_real_hash;
    } else {
        prefix = ctx->cons_managed_path;
        can_prefix = ctx->cons_real_path;
        prefix_len = ctx->cons_managed_len;
        can_prefix_len = ctx->cons_real_len;
        prefix_hash = ctx->cons_managed_hash;
        can_prefix_hash = ctx->cons_real_hash;
    }

    const uint32_t path_hash1 = hash_path_prefix (path, DYAD_SEED, prefix_len);

    if ((path_hash1 == prefix_hash)
        && extract_user_path (prefix, path, DYAD_PATH_DELIM, upath, upath_capacity)) {
        return true;
    }

    if (can_prefix_len > 0u) {
        const uint32_t path_hash2 = hash_path_prefix (path, DYAD_SEED, can_prefix_len);
        if (path_hash2 == can_prefix_hash) {
            if (extract_user_path (can_prefix, path, DYAD_PATH_DELIM, upath, upath_capacity)) {
                return true;
            }
        }
    }

    // See if the prefix of the path in question matches that of either the
    // dyad managed path or its canonical form when the path is not a real one.
    if (!realpath (path, can_path)) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: %s does not include prefix %s.\n", path, prefix);
        return false;
    }

    const uint32_t can_path_hash1 = hash_path_prefix (can_path, DYAD_SEED, prefix_len);

    if ((can_path_hash1 == prefix_hash)
        && extract_user_path (prefix, can_path, DYAD_PATH_DELIM, upath, upath_capacity)) {
        return true;
    }

    if (can_prefix_len > 0u) {
        const uint32_t can_path_hash2 = hash_path_prefix (can_path, DYAD_SEED, can_prefix_len);
        if ((can_path_hash2 == can_prefix_hash)
            && extract_user_path (can_prefix, can_path, DYAD_PATH_DELIM, upath, upath_capacity)) {
            return true;
        }
    }

    return false;
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
                DYAD_LOG_DEBUG (NULL, "\"%s\" already exists not as a directory\n", path);
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
        DYAD_LOG_DEBUG (NULL,
                        "DYAD UTIL: error reading the file link (%s): %s\n",
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

dyad_rc_t dyad_excl_flock (const dyad_ctx_t* __restrict__ ctx,
                           int fd,
                           struct flock* __restrict__ lock)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] Applies an exclusive lock on fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto excl_flock_end;
    }
    lock->l_type = F_WRLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid;                  // getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) {  // will wait until able to lock
        DYAD_LOG_ERROR (ctx, "Cannot apply exclusive lock on fd %d\n", fd);
        rc = DYAD_RC_BADFIO;
        goto excl_flock_end;
    }
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] Exclusive lock placed on fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    rc = DYAD_RC_OK;
excl_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_shared_flock (const dyad_ctx_t* __restrict__ ctx,
                             int fd,
                             struct flock* __restrict__ lock)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] Applies a shared lock on fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto shared_flock_end;
    }
    lock->l_type = F_RDLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid;                  // getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) {  // will wait until able to lock
        DYAD_LOG_ERROR (ctx, "Cannot apply shared lock on fd %d\n", fd);
        rc = DYAD_RC_BADFIO;
        goto shared_flock_end;
    }
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] Shared lock placed on fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    rc = DYAD_RC_OK;
shared_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_release_flock (const dyad_ctx_t* __restrict__ ctx,
                              int fd,
                              struct flock* __restrict__ lock)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] Releases a lock on fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    lock->l_type = F_UNLCK;
    if (fcntl (fd, F_SETLKW, lock) == -1) {  // will just unlock
        DYAD_LOG_ERROR (ctx, "Cannot release lock on fd %d\n", fd);
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    DYAD_LOG_INFO (ctx,
                   "[node %u rank %u pid %d] lock lifted from fd %d\n",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   fd);
    rc = DYAD_RC_OK;
release_flock_end:;
    DYAD_C_FUNCTION_END ();
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
        snprintf (errmsg, PATH_MAX + 256, "Failed to open directory %s\n", containing_dir);
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
