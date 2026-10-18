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

uint32_t hash_str (const char *str, const uint32_t seed)
{
    if (!str)
        return 0u;
    const char *str_long = str;
    size_t str_len = strlen (str);
    char buf[256] = {'\0'};
    uint32_t hash[4] = {0u};  // Output for the hash

    if (str_len == 0ul)
        return 0u;

    // Just append the string so that it can be as large as 128 bytes.
    if (str_len < 128ul) {
        memcpy (buf, str, str_len);
        memset (buf + str_len, '@', 128ul - str_len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    MurmurHash3_x64_128 (str_long, str_len, seed, hash);
    return (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) + 1;
}

uint32_t hash_path_prefix (const char *str, const uint32_t seed, const size_t len)
{
    if (!str || len == 0ul) {
        return 0u;
    }
    const char *str_long = str;
    size_t str_len = strlen (str);
    char buf[256] = {'\0'};
    uint32_t hash[4] = {0u};  // Output for the hash

    if (str_len < len)
        return 0u;
    str_len = len;

    // Just append the string so that it can be as large as 128 bytes.
    if (len < 128ul) {
        memcpy (buf, str, len);
        memset (buf + len, '@', 128ul - len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    MurmurHash3_x64_128 (str_long, str_len, seed, hash);
    return (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) + 1;
}

char *concat_str (char *DYAD_RESTRICT str,
                  const char *DYAD_RESTRICT to_append,
                  const char *DYAD_RESTRICT connector,
                  size_t str_capacity)
{
    const size_t str_len_org = strlen (str);
    const size_t con_len = strlen (connector);
    const size_t app_len = strlen (to_append);
    const size_t all_len = str_len_org + con_len + app_len;

    if (all_len + 1ul > str_capacity) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffer is too small.");
        return NULL;
    }

    bool con_end = false;
    if ((connector != NULL) && (str_len_org >= con_len)) {
        con_end = (strncmp (str + str_len_org - con_len, connector, con_len) == 0);
    }
    const size_t str_len = (con_end ? (str_len_org - con_len) : str_len_org);

    const char *const str_end = str + str_capacity;
    bool no_overlap = ((to_append + strlen (to_append) <= str) || (str_end <= to_append))
                      && ((connector + strlen (connector) <= str) || (str_end <= connector));

    if (!no_overlap) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.");
        return NULL;
    }

    char *buf = (char *)calloc (all_len + 1ul, sizeof (char));

    memcpy (buf, str, str_len);
    char *buf_pos = buf + str_len;

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

bool extract_user_path (const char *DYAD_RESTRICT prefix,
                        const char *DYAD_RESTRICT full,
                        const char *DYAD_RESTRICT delim,
                        char *DYAD_RESTRICT upath,
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
        const char *const upath_end = upath + upath_capacity;
        bool no_overlap = ((prefix + prefix_len <= upath) || (upath_end <= prefix))
                          && ((full + full_len <= upath) || (upath_end <= full))
                          && ((delim + delim_len <= upath) || (upath_end <= delim));

        if (!no_overlap) {
            DYAD_LOG_DEBUG (NULL, "DYAD UTIL: buffers overlap.");
            return false;
        }
    }

    // Check if the full path begins with the prefix string
    if (strncmp (prefix, full, prefix_len) != 0) {
        return false;
    }

    if (full_len > PATH_MAX || delim_len > PATH_MAX || prefix_len > PATH_MAX) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: path length cannot be larger than PATH_MAX.");
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

bool cmp_canonical_path_prefix (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                const bool is_prod,
                                const char *DYAD_RESTRICT path,
                                char *DYAD_RESTRICT upath,
                                const size_t upath_capacity)
{
    // Only works when there are no multiple absolute paths via hardlinks
    char can_path[PATH_MAX] = {'\0'};  // canonical form of the given path
    if (!ctx) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: Invalid dyad context!\n");
        return false;
    }

    const char *prefix = NULL;
    const char *can_prefix = NULL;
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
 * @brief Recursively creates a directory and all missing parent directories.
 *
 * @details
 * Creates @p dir and any intermediate parent directories that do not yet exist,
 * similar to @c mkdir -p. If @p dir already exists, returns 0 immediately
 * without error.
 *
 * The implementation recurses up the directory tree via @c dirname() until it
 * reaches a directory that already exists, then creates each missing component
 * on the way back down. @c strdupa() is used to duplicate the path before
 * passing it to @c dirname() since @c dirname() may modify its argument in
 * place.
 *
 * See https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix
 * for the basis of this implementation.
 *
 * @param[in] dir  Null-terminated path of the directory to create. Must not
 *                 be @c NULL. If @c NULL, sets @c errno to @c EINVAL and
 *                 returns 1.
 * @param[in] m    Permission mode bits to apply to each newly created
 *                 directory, passed directly to @c mkdir().
 *
 * @return int
 * @retval 0        @p dir already exists or was successfully created along
 *                  with all required parent directories.
 * @retval 1        @p dir is @c NULL (@c errno set to @c EINVAL).
 * @retval non-zero The return value of @c mkdir() for the final directory
 *                  component if creation failed, with @c errno set by
 *                  @c mkdir().
 *
 * @note The permission mode @p m is applied to each directory created during
 *       the recursive descent. The effective permissions may differ from @p m
 *       depending on the process @c umask.
 * @note This function uses @c strdupa() which allocates on the stack. Deep
 *       directory hierarchies or very long paths may cause stack overflow.
 * @warning Return codes from intermediate @c mkdir() calls during recursion
 *          are not checked. Only the return value of the final @c mkdir() for
 *          @p dir itself is returned to the caller.
 */
int mkpath (const char *dir, const mode_t m)
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

int mkdir_as_needed (const char *path, const mode_t m)
{
    if (path == NULL || strlen (path) == 0ul) {
        DYAD_LOG_ERROR (NULL, "DYAD UTL: Cannot create a directory with no name\n");
        return -3;
    }

    struct stat sb;

    const mode_t RWX_UGO = (S_IRWXU | S_IRWXG | S_IRWXO);

    if (stat (path, &sb) == 0) {          // already exist
        if (S_ISDIR (sb.st_mode) == 0) {  // not a directory
            DYAD_LOG_DEBUG (NULL, "DYAD UTIL: \"%s\" already exists not as a directory\n", path);
            return -2;
        } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
            DYAD_LOG_DEBUG (NULL,
                            "DYAD UTIL: Directory \"%s\" already exists with "
                            "different permission bits %o from "
                            "the requested %o.",
                            path,
                            (sb.st_mode & RWX_UGO),
                            (m & RWX_UGO));
            return 5;  // already exists but with different mode
        }
        return 1;  // already exists
    }
    DYAD_LOG_DEBUG (NULL, "DYAD UTIL: Creating directory \"%s\".", path);

    const mode_t old_mask = umask (0);
    if (mkpath (path, m) != 0) {
        if (stat (path, &sb) == 0) {          // already exist
            if (S_ISDIR (sb.st_mode) == 0) {  // not a directory
                DYAD_LOG_DEBUG (NULL, "DYAD UTIL: \"%s\" already exists not as a directory.", path);
                umask (old_mask);
                return -4;
            } else if ((sb.st_mode & RWX_UGO) ^ (m & RWX_UGO)) {
                DYAD_LOG_DEBUG (NULL,
                                "DYAD UTIL: Directory \"%s\" already exists with "
                                "different permission bits %o from "
                                "the requested %o.",
                                path,
                                (sb.st_mode & RWX_UGO),
                                (m & RWX_UGO));
                umask (old_mask);
                return 5;  // already exists but with different mode
            }
            umask (old_mask);
            return 1;  // already exists
        }
        DYAD_LOG_DEBUG (NULL,
                        "DYAD UTIL: Cannot create directory \"%s\": %s.",
                        path,
                        strerror (errno));
        perror ("mkdir_as_needed() ");
        umask (old_mask);
        return -1;
    }
    umask (old_mask);

#if DYAD_SYNC_DIR
    sync_containing_dir (path);
#endif  // DYAD_SYNC_DIR

    return 0;  // The new directory has been succesfully created
}

int get_path (const int fd, const size_t max_size, char *path)
{
    if (max_size < 1ul) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: Invalid max path size.");
        return -1;
    }
    memset (path, '\0', max_size + 1);
    char proclink[PATH_MAX] = {'\0'};

    sprintf (proclink, "/proc/self/fd/%d", fd);
    ssize_t rc = readlink (proclink, path, max_size);
    if (rc < (ssize_t)0) {
        DYAD_LOG_DEBUG (NULL,
                        "DYAD UTIL: error reading the file link (%s): %s.",
                        strerror (errno),
                        proclink);
        return -1;
    } else if ((size_t)rc == max_size) {
        DYAD_LOG_DEBUG (NULL, "DYAD UTIL: truncation might have happend with %s.", proclink);
    }
    path[max_size] = '\0';

    return 0;
}

bool is_path_dir (const char *path)
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
bool get_stat (const char *path, unsigned int max_retry, long ns_sleep)
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

#if 0
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
#else
ssize_t get_file_size (int fd)
{
    struct stat st;
    if (fstat (fd, &st) == -1) {
        return 0l;  // errno already set by fstat
    }
    return (ssize_t)(st.st_size);
}
#endif

dyad_rc_t dyad_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                           int fd,
                           struct flock *DYAD_RESTRICT lock)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    DYAD_WORKER_LOG_DEBUG (
                    "DYAD UTIL: [node %u rank %u pid %d] Applies an exclusive lock on fd %d.",
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
        DYAD_WORKER_LOG_ERROR ("DYAD UTIL: Cannot apply exclusive lock on fd %d.", fd);
        rc = DYAD_RC_BADFIO;
        goto excl_flock_end;
    }
    rc = DYAD_RC_OK;
excl_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_try_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                               int fd,
                               struct flock *DYAD_RESTRICT lock)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    DYAD_LOG_DEBUG (ctx,
                    "DYAD UTIL: [node %u rank %u pid %d] Attempts a non-blocking exclusive lock "
                    "on fd %d.",
                    ctx->node_idx,
                    ctx->rank,
                    ctx->pid,
                    fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto try_excl_flock_end;
    }
    lock->l_type = F_WRLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid;                 // getpid();
    if (fcntl (fd, F_SETLK, lock) == -1) {  // returns immediately instead of waiting
        if (errno == EAGAIN || errno == EACCES) {
            rc = DYAD_RC_BUSY;
        } else {
            DYAD_LOG_ERROR (ctx, "DYAD UTIL: Cannot apply exclusive lock on fd %d.", fd);
            rc = DYAD_RC_BADFIO;
        }
        goto try_excl_flock_end;
    }
    rc = DYAD_RC_OK;
try_excl_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_shared_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                             int fd,
                             struct flock *DYAD_RESTRICT lock)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_WORKER_LOG_DEBUG (
        "DYAD UTIL: [node %u rank %u pid %d] Applies a shared lock on fd %d.",
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
        DYAD_WORKER_LOG_ERROR ("DYAD UTIL: Cannot apply shared lock on fd %d.", fd);
        rc = DYAD_RC_BADFIO;
        goto shared_flock_end;
    }
    rc = DYAD_RC_OK;
shared_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_release_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                              int fd,
                              struct flock *DYAD_RESTRICT lock)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_WORKER_LOG_DEBUG ("DYAD UTIL: [node %u rank %u pid %d] Releases a lock on fd %d.",
                           ctx->node_idx,
                           ctx->rank,
                           ctx->pid,
                           fd);
    if (!lock) {
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    // Always unlock the whole file, matching how dyad_excl_flock() /
    // dyad_shared_flock() acquire it.  Callers on the close()/fclose()
    // wrapper paths pass a struct flock that was never used to acquire a
    // lock, so its l_whence/l_start/l_len would otherwise be uninitialised
    // when handed to fcntl().
    lock->l_type = F_UNLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid;
    if (fcntl (fd, F_SETLKW, lock) == -1) {  // will just unlock
        DYAD_WORKER_LOG_ERROR ("DYAD UTIL: Cannot release lock on fd %d.", fd);
        rc = DYAD_RC_BADFIO;
        goto release_flock_end;
    }
    rc = DYAD_RC_OK;
release_flock_end:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

#if DYAD_SYNC_DIR
int sync_containing_dir (const char *path)
{
    char fullpath[PATH_MAX + 2] = {'\0'};
    strncpy (fullpath, path, PATH_MAX);
    const char *containing_dir = dirname (fullpath);
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
                  "DYAD UTIL: Failed to open directory %s.\n",
                  containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    if (fsync (dir_fd) < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg, PATH_MAX + 256, "DYAD UTIL: Failed to fsync %s.\n", containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    if (close (dir_fd) < 0) {
        char errmsg[PATH_MAX + 256] = {'\0'};
        snprintf (errmsg, PATH_MAX + 256, "DYAD UTIL: Failed to close %s.\n", containing_dir);
        perror (errmsg);
        return -1;  // exit (SYS_ERR);
    }

    return 0;
}
#endif  // DYAD_SYNC_DIR
