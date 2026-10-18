/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_UTILS_RANGE_CACHE_H
#define DYAD_UTILS_RANGE_CACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#if defined(__cplusplus)
#include <cstddef>
#else
#include <stdbool.h>
#include <stddef.h>
#endif  // defined(__cplusplus)

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures_int.h>

#if defined(__cplusplus)
extern "C" {
#endif  // defined(__cplusplus)

/**
 * @brief Block size, in bytes, used to track which spans of a locally
 *        cached file are present.
 *
 * @details
 * Chosen to be a few times larger than the typical byte-range request
 * (~50-200KB atom/edge spans in the PECAN workload) so that most requests
 * span only 1-3 blocks, keeping block-edge over-fetch small relative to
 * request size, while still keeping the tracking bitmap small (a 15GB
 * shard is ~234K blocks, i.e. ~29KB of bitmap).
 */
#define DYAD_RANGE_CACHE_BLOCK_SIZE (64UL * 1024UL)

/**
 * @brief Ensures that @c [offset, offset+length) of @p local_path is
 *        present locally, lazily fetching any missing block-aligned span
 *        from @p origin_path first.
 *
 * @details
 * Tracks which @c DYAD_RANGE_CACHE_BLOCK_SIZE-aligned blocks of
 * @p local_path have already been populated using a bitmap stored in a
 * companion file (@c "<local_path>.dyad_cached"), sized lazily on first
 * touch from @c stat(origin_path). Blocks are filled in write-through: on
 * a miss, the block-aligned span covering @c [offset, offset+length) is
 * read from @p origin_path and written into @p local_path at the same
 * offset, and the corresponding bits are set so later requests (from any
 * process sharing @p local_path, local or remote) hit the cache instead
 * of re-reading the origin.
 *
 * Coordination across concurrent callers uses @c dyad_shared_flock() /
 * @c dyad_excl_flock() on the bitmap file (not on @p local_path itself,
 * so large reads/writes to the data file are never serialized by the
 * bookkeeping lock): a shared lock is used to check for a full hit, and
 * only escalates to an exclusive lock (with a re-check, since another
 * process may have filled the span while this one waited) on a miss.
 *
 * This function does not read data into a caller-supplied buffer itself
 * -- once it returns @c DYAD_RC_OK, the caller is expected to read the
 * requested range from @p local_path exactly as it would if the file had
 * been fully staged upfront (e.g. via @c pread()).
 *
 * @param[in] ctx          DYAD context. Must not be @c NULL.
 * @param[in] local_path   Path to the locally cached copy of the file.
 *                         Created (and sized to match @p origin_path) on
 *                         first touch if it does not already exist.
 * @param[in] origin_path  Path to the authoritative source (e.g. on the
 *                         parallel file system) to fetch missing spans
 *                         from. If @c NULL, this function is a no-op that
 *                         returns @c DYAD_RC_OK immediately, preserving
 *                         the "already fully staged" behavior for callers
 *                         that don't opt in to lazy caching.
 * @param[in] offset       Byte offset of the requested range within the
 *                         file.
 * @param[in] length       Length, in bytes, of the requested range. If 0,
 *                         this function is a no-op that returns
 *                         @c DYAD_RC_OK immediately.
 *                         If the range extends past the end of
 *                         @p origin_path, only the portion within the
 *                         source file is cached.
 *
 * @return @c dyad_rc_t    Return code indicating the outcome:
 * @retval DYAD_RC_OK      @p origin_path is @c NULL, @p length is 0, or
 *                         @c [offset, offset+length) is now present in
 *                         @p local_path (either because it already was,
 *                         or because it was just fetched from
 *                         @p origin_path).
 * @retval DYAD_RC_BADFIO  A filesystem operation (open/stat/read/write)
 *                         on @p local_path, its bitmap file, or
 *                         @p origin_path failed; or @p offset is outside
 *                         the source file or the requested range overflows.
 * @retval DYAD_RC_SYSFAIL A memory allocation failed.
 * @retval DYAD_RC_BADBUF  @p local_path is too long to build the bitmap
 *                         file's path.
 */
dyad_rc_t dyad_range_cache_ensure (const dyad_ctx_t *ctx,
                                   const char *local_path,
                                   const char *origin_path,
                                   size_t offset,
                                   size_t length);

/**
 * @brief Reports whether a range-cache bitmap marks the complete local file
 *        as present.
 *
 * @param[in]  ctx              DYAD context used for synchronization.
 * @param[in]  local_path       Path of the local data file.
 * @param[out] has_range_cache  True when @p local_path has a range-cache
 *                               bitmap; false for an ordinary local file.
 * @param[out] complete         True when every block of a range-cached file
 *                               is marked present. True for an ordinary
 *                               local file.
 *
 * @return @c DYAD_RC_OK on success, @c DYAD_RC_BADBUF if the bitmap path is
 *         too long, @c DYAD_RC_BADFIO for a filesystem failure, or
 *         @c DYAD_RC_SYSFAIL if allocation fails.
 */
dyad_rc_t dyad_range_cache_is_complete (const dyad_ctx_t *ctx,
                                        const char *local_path,
                                        bool *has_range_cache,
                                        bool *complete);

/**
 * @brief Marks every block in an existing range-cache bitmap as present.
 *
 * @param[in] ctx        DYAD context used for synchronization.
 * @param[in] local_path Path of a fully populated local data file.
 *
 * @return @c DYAD_RC_OK on success or when no bitmap exists,
 *         @c DYAD_RC_BADBUF if the bitmap path is too long,
 *         @c DYAD_RC_BADFIO for a filesystem failure, or @c DYAD_RC_SYSFAIL
 *         if allocation fails.
 */
dyad_rc_t dyad_range_cache_mark_complete (const dyad_ctx_t *ctx, const char *local_path);

#if defined(__cplusplus)
}
#endif  // defined(__cplusplus)

#endif  // DYAD_UTILS_RANGE_CACHE_H
