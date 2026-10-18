/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_UTILS_UTILS_H
#define DYAD_UTILS_UTILS_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

#if defined(__cplusplus)
// #include <cstdbool> // c++11
#include <cstddef>
#include <cstdint>
#include <cstdio>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#endif  // defined(__cplusplus)

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures_int.h>
#include <fcntl.h>

#if defined(__cplusplus)
extern "C" {
#endif  // defined(__cplusplus)

bool file_in_read_mode (FILE *f);
bool fd_in_read_mode (int fd);
bool oflag_is_read (int oflag);
bool mode_is_read (const char *mode);

void enable_debug_dyad_utils (void);
void disable_debug_dyad_utils (void);
bool check_debug_dyad_utils (void);

/**
 * @brief Computes a non-zero MurmurHash3 hash of a string.
 *
 * @details
 * Hashes @p str using MurmurHash3_x64_128 with the given @p seed, then
 * reduces the four 32-bit output words via XOR and adds 1 to ensure the
 * result is never zero. This allows zero to be used unambiguously as an
 * error sentinel by the caller.
 *
 * If @p str is shorter than 128 bytes, it is padded with @c '@' characters
 * to 128 bytes before hashing to improve hash distribution for short strings,
 * consistent with the padding used in @c gen_path_key().
 *
 * @param[in] str   Null-terminated string to hash. If @c NULL or empty,
 *                  returns 0.
 * @param[in] seed  Seed value for MurmurHash3.
 *
 * @return uint32_t
 * @retval 0        @p str is @c NULL or empty.
 * @retval non-zero The computed hash value.
 */
uint32_t hash_str (const char *str, const uint32_t seed);

/**
 * @brief Computes a non-zero MurmurHash3 hash of the first @p len bytes of a string.
 *
 * @details
 * Hashes only the first @p len bytes of @p str using MurmurHash3_x64_128 with
 * the given @p seed, then reduces the four 32-bit output words via XOR and adds
 * 1 to ensure the result is never zero. This allows zero to be used unambiguously
 * as an error sentinel by the caller.
 *
 * If @p len is shorter than 128 bytes, the prefix is padded with @c '@' characters
 * to 128 bytes before hashing to improve hash distribution for short prefixes,
 * consistent with the padding used in @c hash_str() and @c gen_path_key().
 *
 * If @p str is shorter than @p len bytes, the function returns 0 since the
 * requested prefix length exceeds the actual string length.
 *
 * @param[in] str   Null-terminated string whose prefix is to be hashed. If
 *                  @c NULL, returns 0.
 * @param[in] seed  Seed value for MurmurHash3.
 * @param[in] len   Number of bytes to hash from the start of @p str. If 0 or
 *                  greater than @c strlen(str), returns 0.
 *
 * @return uint32_t
 * @retval 0        @p str is @c NULL, @p len is 0, or @p len exceeds the
 *                  length of @p str.
 * @retval non-zero The computed hash value of the first @p len bytes.
 */
uint32_t hash_path_prefix (const char *str, const uint32_t seed, const size_t len);

/**
 * @brief Appends a string to an existing buffer, joining them with a connector.
 *
 * @details
 * Concatenates @p connector and @p to_append onto the end of @p str in-place,
 * producing @c "str + connector + to_append". The result is written back into
 * the @p str buffer.
 *
 * If @p str already ends with @p connector, the trailing connector is stripped
 * before appending to avoid duplicating it. For example, concatenating @c "foo/"
 * with connector @c "/" and @p to_append @c "bar" produces @c "foo/bar" rather
 * than @c "foo//bar".
 *
 * The operation is performed via an intermediate heap-allocated buffer to
 * safely handle the in-place update of @p str. If @p str, @p to_append, or
 * @p connector overlap in memory, the function returns @c NULL without
 * modifying @p str.
 *
 * @param[in,out] str           Null-terminated string to append to. Also serves
 *                              as the output buffer. Must not be @c NULL and must
 *                              be at least @p str_capacity bytes in size.
 * @param[in]     to_append     Null-terminated string to append. Must not be
 *                              @c NULL and must not overlap with @p str.
 * @param[in]     connector     Null-terminated string to insert between @p str
 *                              and @p to_append (e.g. @c "/"). Must not overlap
 *                              with @p str.
 * @param[in]     str_capacity  Total size of the @p str buffer in bytes. The
 *                              combined result must fit within this capacity
 *                              including the null terminator.
 *
 * @return char*
 * @retval str   The operation succeeded and @p str now contains the concatenated
 *               result.
 * @retval NULL  The combined result would exceed @p str_capacity, or @p str,
 *               @p to_append, and @p connector overlap in memory.
 *
 * @note This function allocates a temporary heap buffer internally for the
 *       concatenation and frees it before returning.
 */
char *concat_str (char *DYAD_RESTRICT str,
                  const char *DYAD_RESTRICT to_append,
                  const char *DYAD_RESTRICT connector,
                  size_t str_capacity);

/**
 * @brief Extracts the path component following a managed directory prefix.
 *
 * @details
 * Checks whether @p full begins with @p prefix (separated by @p delim) and,
 * if so, extracts the portion of @p full that follows the prefix and delimiter
 * into @p upath. This is used to derive the path of a file relative to a
 * DYAD-managed directory from its absolute path.
 *
 * For example, with @p prefix @c "/managed", @p delim @c "/", and @p full
 * @c "/managed/subdir/file.txt", the extracted @p upath would be
 * @c "subdir/file.txt".
 *
 * The following conditions all cause the function to return @c false without
 * modifying @p upath:
 *  - @p upath is @c NULL.
 *  - @p prefix, @p full, or @p delim overlaps with the @p upath buffer.
 *  - @p full does not begin with @p prefix.
 *  - Any path argument exceeds @c PATH_MAX bytes.
 *  - @p full is equal to @p prefix with no user path component following it.
 *  - The delimiter is not present between @p prefix and the user path in
 *    @p full (e.g. @c "/managed_other/file" does not match prefix
 *    @c "/managed").
 *  - The extracted user path would exceed @p upath_capacity bytes including
 *    the null terminator.
 *
 * If @p prefix itself ends with @p delim, the trailing delimiter is stripped
 * before matching to avoid requiring a double delimiter between the prefix and
 * the user path.
 *
 * @param[in]  prefix          Null-terminated managed directory path to match
 *                             against the start of @p full. Must not be @c NULL.
 * @param[in]  full            Null-terminated absolute file path to extract from.
 *                             Must not be @c NULL and must not overlap with
 *                             @p upath.
 * @param[in]  delim           Null-terminated path delimiter string (e.g. @c "/").
 *                             If @c NULL, treated as an empty string.
 * @param[out] upath           Buffer to receive the extracted relative path.
 *                             Must not be @c NULL or overlap with any other
 *                             argument. Not null-terminated by this function;
 *                             the caller should ensure the buffer is zeroed
 *                             before calling.
 * @param[in]  upath_capacity  Size of the @p upath buffer in bytes. The
 *                             extracted path must fit within this capacity
 *                             including a null terminator.
 *
 * @return bool
 * @retval true   @p full begins with @p prefix and the relative path was
 *                successfully extracted into @p upath.
 * @retval false  Any of the failure conditions listed above were met. @p upath
 *                is not modified.
 *
 * @note @p upath is not explicitly null-terminated by this function. Callers
 *       should zero-initialize the buffer before calling to ensure the result
 *       is null-terminated.
 */
bool extract_user_path (const char *DYAD_RESTRICT prefix,
                        const char *DYAD_RESTRICT full,
                        const char *DYAD_RESTRICT delim,
                        char *DYAD_RESTRICT upath,
                        const size_t upath_capacity);

/**
 * @brief Checks whether a path falls under a DYAD-managed directory and
 *        extracts its relative component.
 *
 * @details
 * Determines if @p path is under the DYAD-managed directory for either the
 * producer (@p is_prod is @c true) or consumer (@p is_prod is @c false), and
 * if so, extracts the portion of @p path following the managed prefix into
 * @p upath.
 *
 * To handle symlinks and non-canonical paths, the check is attempted in up
 * to four passes before returning @c false:
 *  1. Hash and match @p path against the managed path prefix.
 *  2. Hash and match @p path against the canonical (real) managed path prefix,
 *     if one is available (@c can_prefix_len > 0).
 *  3. Resolve @p path to its canonical form via @c realpath(), then hash and
 *     match the result against the managed path prefix.
 *  4. Hash and match the canonical form of @p path against the canonical
 *     managed path prefix.
 *
 * Each pass first compares a hash of the appropriate prefix-length of the path
 * against the pre-computed prefix hash stored in the context, and only calls
 * @c extract_user_path() on a hash match. This avoids the cost of full string
 * comparison for paths that clearly do not match.
 *
 * @p upath is populated by the first passing match and the function returns
 * immediately without attempting further passes.
 *
 * @param[in]  ctx             Pointer to the DYAD context. Must not be @c NULL.
 *                             Provides the managed path, its canonical form,
 *                             their lengths, and their pre-computed hashes for
 *                             both producer and consumer sides.
 * @param[in]  is_prod         If @c true, match against the producer-managed
 *                             path (@c ctx->prod_managed_path). If @c false,
 *                             match against the consumer-managed path
 *                             (@c ctx->cons_managed_path).
 * @param[in]  path            Null-terminated path to check. May be a symlink
 *                             or non-canonical path; @c realpath() is used as
 *                             a fallback if direct matching fails.
 * @param[out] upath           Buffer to receive the relative path component
 *                             following the managed prefix. Should be
 *                             zero-initialized by the caller. Not explicitly
 *                             null-terminated by this function.
 * @param[in]  upath_capacity  Size of the @p upath buffer in bytes.
 *
 * @return bool
 * @retval true   @p path (or its canonical form) is under the managed directory
 *                and the relative component has been written to @p upath.
 * @retval false  @p ctx is @c NULL, @p path does not fall under the managed
 *                directory under any of the four matching passes, or
 *                @c realpath() failed when resolving @p path.
 *
 * @note The function assumes that the prefix lengths (@c prod_managed_len,
 *       @c cons_managed_len, etc.) and pre-computed hashes stored in @p ctx
 *       are accurate and consistent with the corresponding path strings. No
 *       internal validation of these values is performed.
 * @note Hash collisions between an unrelated path and a managed prefix will
 *       cause @c extract_user_path() to be called unnecessarily, but the full
 *       string comparison inside @c extract_user_path() will correctly reject
 *       the mismatch.
 * @note This function only works correctly when there are no multiple absolute
 *       paths to the same file via hard links.
 */
bool cmp_canonical_path_prefix (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                const bool is_prod,
                                const char *DYAD_RESTRICT path,
                                char *DYAD_RESTRICT upath,
                                const size_t upath_capacity);

/**
 * @brief Creates a directory and all missing parent directories, with
 *        existence and permission checks.
 *
 * @details
 * Creates @p path and any missing intermediate parent directories using
 * @c mkpath(). Before attempting creation, checks whether @p path already
 * exists and validates that it is a directory with the expected permission
 * bits. The same checks are repeated after @c mkpath() returns a non-zero
 * value, since a concurrent process may have created the directory in the
 * interim.
 *
 * The process @c umask is temporarily set to 0 during directory creation to
 * ensure that the permission bits specified by @p m are applied exactly as
 * requested. The original @c umask is restored after @c mkpath() returns.
 *
 * If @c DYAD_SYNC_DIR is defined at compile time, the parent directory of
 * @p path is synced via @c sync_containing_dir() after successful creation
 * to ensure the new directory entry is durable on storage.
 *
 * @param[in] path  Null-terminated path of the directory to create. Must not
 *                  be @c NULL or empty.
 * @param[in] m     Permission mode bits to apply to newly created directories.
 *                  The @c umask is set to 0 during creation so these bits are
 *                  applied exactly.
 *
 * @return int
 * @retval  0   The directory was successfully created.
 * @retval  1   The directory already exists with the requested permissions.
 * @retval  5   The directory already exists but with different permission bits.
 * @retval -1   @c mkpath() failed and the directory does not exist afterward.
 * @retval -2   @p path already exists but is not a directory.
 * @retval -3   @p path is @c NULL or empty.
 * @retval -4   @c mkpath() failed but a subsequent @c stat() found @p path
 *              exists as a non-directory entry.
 *
 * @note The @c umask is restored to its original value after @c mkpath()
 *       returns, but is not restored if @c mkpath() is interrupted abnormally.
 * @note Return code 5 is not an error in the strict sense — the directory is
 *       usable — but callers may wish to log or handle the permission mismatch
 *       depending on their security requirements.
 * @warning This function calls @c perror() directly on @c mkpath() failure,
 *          which writes to @c stderr. Callers that manage their own error
 *          output should be aware of this side effect.
 */
int mkdir_as_needed (const char *path, const mode_t m);

/**
 * @brief Resolves the file path associated with an open file descriptor.
 *
 * @details
 * Reads the symbolic link @c /proc/self/fd/ followed by @p fd via @c readlink() to obtain
 * the path of the file currently open on @p fd, and writes the result into
 * @p path. This is a Linux-specific mechanism and requires @c /proc to be
 * mounted.
 *
 * @p path is zero-initialized up to @p max_size + 1 bytes before the
 * @c readlink() call. If @c readlink() returns exactly @p max_size bytes,
 * a truncation warning is logged since the path may have been silently
 * truncated.
 *
 * @param[in]  fd        Open file descriptor whose path is to be resolved.
 * @param[in]  max_size  Maximum number of bytes to write into @p path,
 *                       excluding the null terminator. Must be at least 1.
 *                       The @p path buffer must be at least @p max_size + 1
 *                       bytes in size to accommodate the null terminator.
 * @param[out] path      Buffer to receive the resolved path. Zero-initialized
 *                       by this function up to @p max_size + 1 bytes before
 *                       the @c readlink() call.
 *
 * @return int
 * @retval  0  The path was successfully resolved and written to @p path.
 * @retval -1  @p max_size is less than 1, or @c readlink() failed
 *             (@c errno set by @c readlink()).
 *
 * @note If @c readlink() returns exactly @p max_size bytes, the path may have
 *       been truncated. A debug message is logged but the function still
 *       returns 0. Callers that require exact paths should use a buffer of
 *       at least @c PATH_MAX + 1 bytes.
 * @note This function relies on @c /proc/self/fd/, which is Linux-specific
 *       and requires @c /proc to be mounted.
 * @warning There is an off-by-one issue in the null terminator placement:
 *          @c path[max_size + 1] is written rather than @c path[max_size],
 *          which writes one byte past the end of a @p max_size + 1 sized
 *          buffer. The @p path buffer should be at least @p max_size + 2
 *          bytes to avoid a buffer overwrite.
 */
int get_path (const int fd, const size_t max_size, char *path);

/// Check if the path is a directory
bool is_path_dir (const char *path);

/**
 * @brief Checks whether an open file descriptor refers to a directory.
 *
 * @param[in] fd  File descriptor to check. If negative, returns @c false
 *                immediately without calling @c fstat().
 *
 * @return bool
 * @retval true   @p fd is a valid open file descriptor referring to a directory.
 * @retval false  @p fd is negative, @c fstat() failed, or @p fd does not
 *                refer to a directory.
 */
bool is_fd_dir (int fd);

#if DYAD_SPIN_WAIT
/**
 * @brief Checks whether a file exists and is accessible, retrying on failure.
 *
 * @details
 * Calls @c stat() on @p path repeatedly until it succeeds or @p max_retry
 * attempts have been exhausted. A sleep of @p ns_sleep nanoseconds is
 * inserted between attempts. This is useful for waiting on a file that is
 * expected to appear shortly, such as one being written by a concurrent
 * process.
 *
 * @param[in] path       Null-terminated path to the file to check.
 * @param[in] max_retry  Maximum number of @c stat() attempts before giving
 *                       up. If 0, returns @c true immediately without calling
 *                       @c stat().
 * @param[in] ns_sleep   Duration to sleep between retry attempts, in
 *                       nanoseconds. Passed directly to @c nanosleep().
 *
 * @return bool
 * @retval true   @c stat() succeeded within @p max_retry attempts.
 * @retval false  @c stat() failed on all @p max_retry attempts.
 *
 * @note If @p max_retry is 0, the while loop condition is false on the first
 *       evaluation and @c num_retry == max_retry immediately, so the function
 *       returns @c false without ever calling @c stat().
 */
bool get_stat (const char *path, unsigned int max_retry, long ns_sleep);
#endif  // DYAD_SPIN_WAIT

/**
 * @brief Returns the size of an open file in bytes.
 *
 * @details
 * Calls @c fstat() on @p fd to obtain the file size from the file's stat
 * structure. Does not modify the file position and works on any file
 * descriptor for which @c fstat() is supported.
 *
 * @param[in] fd  Open file descriptor to measure.
 *
 * @return ssize_t
 * @retval >=0  The size of the file in bytes. A value of 0 means the file
 *              is empty or @c fstat() failed.
 *
 * @note On a return value of 0, @c errno can be checked to distinguish
 *       between an empty file and a @c fstat() failure. If @c fstat() failed,
 *       @c errno is set to one of:
 *       - @c EBADF:  @p fd is not a valid open file descriptor.
 *       - @c EFAULT: The stat buffer address is invalid (internal error).
 *       - @c EIO:    An I/O error occurred while reading file metadata.
 */
ssize_t get_file_size (int fd);

/**
 * @brief Acquires an exclusive (write) lock on an open file descriptor.
 *
 * @details
 * Sets a POSIX write lock (@c F_WRLCK) over the entire file using @c fcntl()
 * with @c F_SETLKW, blocking the caller until the lock is acquired. This
 * prevents other processes from acquiring any lock (shared or exclusive) on
 * the file until the lock is released via @c dyad_release_flock().
 *
 * If @p lock is @c NULL, the function returns without taking any action.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  fd    File descriptor of the open file to lock.
 * @param[out] lock  Pointer to a @c flock structure populated by this function.
 *                   Must not be @c NULL. The structure is used for subsequent
 *                   unlock calls via @c dyad_release_flock().
 *
 * @return @c dyad_rc_t    Return code indicating the outcome:
 * @retval DYAD_RC_OK      The lock was successfully acquired.
 * @retval DYAD_RC_BADFIO  The @c fcntl() call failed to acquire the lock.
 */
dyad_rc_t dyad_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                           int fd,
                           struct flock *DYAD_RESTRICT lock);
/**
 * @brief Attempts to acquire an exclusive (write) lock without blocking.
 *
 * @details
 * Sets a POSIX write lock (@c F_WRLCK) over the entire file using @c fcntl()
 * with @c F_SETLK (non-blocking), returning immediately instead of waiting
 * if the lock cannot be acquired. Used by the cache evictor to skip a
 * candidate file that is currently locked by another in-flight
 * @c dyad_produce()/@c dyad_consume() call, rather than blocking on it.
 *
 * If @p lock is @c NULL, the function returns without taking any action.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  fd    File descriptor of the open file to lock.
 * @param[out] lock  Pointer to a @c flock structure populated by this function.
 *                   Must not be @c NULL. The structure is used for subsequent
 *                   unlock calls via @c dyad_release_flock().
 *
 * @return @c dyad_rc_t    Return code indicating the outcome:
 * @retval DYAD_RC_OK      The lock was successfully acquired.
 * @retval DYAD_RC_BUSY    The lock is currently held by another process.
 * @retval DYAD_RC_BADFIO  The @c fcntl() call failed for a reason other than
 *                        the lock being held (e.g. bad file descriptor).
 */
dyad_rc_t dyad_try_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                               int fd,
                               struct flock *DYAD_RESTRICT lock);
/**
 * @brief Acquires a shared (read) lock on an open file descriptor.
 *
 * @details
 * Sets a POSIX read lock (@c F_RDLCK) over the entire file using @c fcntl()
 * with @c F_SETLKW, blocking the caller until the lock is acquired. Multiple
 * consumers holding shared locks on the same file may coexist, but a shared
 * lock cannot be acquired while an exclusive lock is held, and vice versa.
 *
 * If @p lock is @c NULL, the function returns without taking any action.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  fd    File descriptor of the open file to lock.
 * @param[out] lock  Pointer to a @c flock structure populated by this function.
 *                   Must not be @c NULL. The structure is used for subsequent
 *                   unlock calls via @c dyad_release_flock().
 *
 * @return @c dyad_rc_t    Return code indicating the outcome:
 * @retval DYAD_RC_OK      The shared lock was successfully acquired.
 * @retval DYAD_RC_BADFIO  The @c fcntl() call failed to acquire the lock.
 */
dyad_rc_t dyad_shared_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                             int fd,
                             struct flock *DYAD_RESTRICT lock);
/**
 * @brief Releases a lock previously acquired on an open file descriptor.
 *
 * @details
 * Clears a POSIX lock (@c F_UNLCK) over the entire file using @c fcntl()
 * with @c F_SETLKW, releasing any lock (exclusive or shared) previously set
 * by @c dyad_excl_flock() or @c dyad_shared_flock(). Other processes blocked
 * on a lock acquisition for this file will be allowed to proceed.
 *
 * If @p lock is @c NULL, the function returns without taking any action.
 *
 * @return @c dyad_rc_t Return code indicating the outcome:
 * @param[in]     ctx   DYAD context.
 * @param[in]     fd    File descriptor of the open file to unlock.
 * @param[in,out] lock  Pointer to the @c flock structure previously populated
 *                      by @c dyad_excl_flock() or @c dyad_shared_flock().
 *                      Must not be @c NULL.
 *
 * @return @c dyad_rc_t    Return code indicating the outcome:
 * @retval DYAD_RC_OK      The shared lock was successfully acquired.
 * @retval DYAD_RC_BADFIO  The @c fcntl() call failed to acquire the lock.
 */
dyad_rc_t dyad_release_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                              int fd,
                              struct flock *DYAD_RESTRICT lock);

/**
 * @brief Acquires an exclusive Linux open-file-description lock for cache I/O.
 *
 * @details
 * Uses @c F_OFD_SETLKW rather than DYAD's traditional process-owned POSIX
 * record locks. The lock belongs to @p fd's open file description, so cache
 * worker threads in one process exclude each other and closing another
 * descriptor for the same inode does not release this lock. OFD locks still
 * conflict with traditional POSIX record locks held by other DYAD paths.
 *
 * @param[in]  ctx  DYAD context used for logging.
 * @param[in]  fd   File descriptor of the open cache file to lock.
 * @param[out] lock Lock description populated for a later release.
 *
 * @retval DYAD_RC_OK     The lock was acquired.
 * @retval DYAD_RC_BADFIO The lock request failed.
 */
dyad_rc_t dyad_cache_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                 int fd,
                                 struct flock *DYAD_RESTRICT lock);

/**
 * @brief Attempts to acquire an exclusive cache OFD lock without waiting.
 *
 * @param[in]  ctx  DYAD context used for logging.
 * @param[in]  fd   File descriptor of the open cache file to lock.
 * @param[out] lock Lock description populated when acquisition succeeds.
 *
 * @retval DYAD_RC_OK     The lock was acquired.
 * @retval DYAD_RC_BUSY   A conflicting lock is held.
 * @retval DYAD_RC_BADFIO The lock request failed.
 */
dyad_rc_t dyad_cache_try_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                     int fd,
                                     struct flock *DYAD_RESTRICT lock);

/**
 * @brief Acquires a shared Linux open-file-description lock for cache I/O.
 *
 * @param[in]  ctx  DYAD context used for logging.
 * @param[in]  fd   File descriptor of the open cache file to lock.
 * @param[out] lock Lock description populated for a later release.
 *
 * @retval DYAD_RC_OK     The lock was acquired.
 * @retval DYAD_RC_BADFIO The lock request failed.
 */
dyad_rc_t dyad_cache_shared_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                   int fd,
                                   struct flock *DYAD_RESTRICT lock);

/**
 * @brief Releases a cache OFD lock acquired through the same descriptor.
 *
 * @param[in]     ctx  DYAD context used for logging.
 * @param[in]     fd   Descriptor that acquired the cache OFD lock.
 * @param[in,out] lock Lock description populated by a cache lock helper.
 *
 * @retval DYAD_RC_OK     The lock was released.
 * @retval DYAD_RC_BADFIO The unlock request failed.
 */
dyad_rc_t dyad_cache_release_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                    int fd,
                                    struct flock *DYAD_RESTRICT lock);

/**
 * @brief Acquires an exclusive Linux open-file-description lock.
 *
 * @param[in]  ctx  DYAD context used for logging.
 * @param[in]  fd   Descriptor whose open file description owns the lock.
 * @param[out] lock Lock description populated for a later release.
 *
 * @retval DYAD_RC_OK     The lock was acquired.
 * @retval DYAD_RC_BADFIO The lock request failed.
 */
dyad_rc_t dyad_ofd_excl_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                               int fd,
                               struct flock *DYAD_RESTRICT lock);

/**
 * @brief Acquires a shared Linux open-file-description lock.
 *
 * @param[in]  ctx  DYAD context used for logging.
 * @param[in]  fd   Descriptor whose open file description owns the lock.
 * @param[out] lock Lock description populated for a later release.
 *
 * @retval DYAD_RC_OK     The lock was acquired.
 * @retval DYAD_RC_BADFIO The lock request failed.
 */
dyad_rc_t dyad_ofd_shared_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                 int fd,
                                 struct flock *DYAD_RESTRICT lock);

/**
 * @brief Releases a Linux open-file-description lock.
 *
 * @param[in]     ctx  DYAD context used for logging.
 * @param[in]     fd   Descriptor whose open file description owns the lock.
 * @param[in,out] lock Lock description populated by an OFD lock helper.
 *
 * @retval DYAD_RC_OK     The lock was released.
 * @retval DYAD_RC_BADFIO The unlock request failed.
 */
dyad_rc_t dyad_ofd_release_flock (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                  int fd,
                                  struct flock *DYAD_RESTRICT lock);

#if DYAD_SYNC_DIR
/**
 * Run fsync for the containing directory of the given path.
 * For example, if path is "/a/b", then fsync on "/a".
 * This cannot be used with DYAD interception.
 */
int sync_containing_dir (const char *path);
#endif  // DYAD_SYNC_DIR

#if defined(__cplusplus)
}
#endif  // defined(__cplusplus)

#endif  // DYAD_UTILS_UTILS_H
