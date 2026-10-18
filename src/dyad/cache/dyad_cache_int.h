#ifndef DYAD_CACHE_DYAD_CACHE_INT_H
#define DYAD_CACHE_DYAD_CACHE_INT_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <limits.h>
#include <sys/types.h>
#include <dyad/cache/dyad_cache_api.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures_int.h>
// clang-format on

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Name of the per-directory advisory lock file used to serialize
 *        the scan-and-evict step across independent processes sharing
 *        one DYAD-managed directory (e.g. DataLoader worker processes).
 */
#define DYAD_CACHE_LOCK_FILENAME ".dyad_cache.lock"

/**
 * @struct dyad_cache_entry
 * @brief One eviction candidate discovered by @c dyad_cache_scan_dir().
 */
struct dyad_cache_entry {
    char path[PATH_MAX + 1];  ///< Full path to the candidate file.
    off_t size;               ///< Allocated size in bytes, from @c st_blocks.
    time_t atime;             ///< Last-access time, from stat() -- used for the grace-period
                              ///< check regardless of which policy is active.
    int64_t recency_key;      ///< Ranking key from the active policy; smaller = evict first.
};

/**
 * @brief Scans a managed directory for eviction candidates.
 *
 * @details
 * Lists every regular file in @p managed_path except the cache lock file
 * itself (@c DYAD_CACHE_LOCK_FILENAME), @c stat()s each one, and
 * populates @p out_entries (caller-owned, must be released with
 * @c free()) with one @c dyad_cache_entry per file, ranked via
 * @c ctx->cache_policy->get_recency_key(). Also reports the total size
 * of all discovered entries via @p out_total_size.
 *
 * @param[in]  ctx            DYAD context. @c ctx->cache_policy must be initialized.
 * @param[in]  managed_path   Directory to scan.
 * @param[out] out_entries    Set to a malloc'd array of entries (caller frees).
 *                            Set to @c NULL if the directory contains no
 *                            eligible entries.
 * @param[out] out_n_entries  Number of entries in @p out_entries.
 * @param[out] out_total_size Sum of @c size across all entries.
 * @return @c DYAD_RC_OK on success, or an error code on failure.
 */
dyad_rc_t dyad_cache_scan_dir (const dyad_ctx_t *ctx,
                               const char *managed_path,
                               struct dyad_cache_entry **out_entries,
                               size_t *out_n_entries,
                               uint64_t *out_total_size);

/**
 * @brief Evicts cached files from a managed directory if it exceeds capacity.
 *
 * @details
 * No-op if @c ctx->cache_capacity_bytes is 0 (eviction disabled, the
 * default) or @c ctx->cache_policy_mode is @c DYAD_CACHE_NONE -- a single
 * branch-and-return, so calling this unconditionally from
 * @c dyad_produce()/@c dyad_consume() carries no overhead for existing
 * deployments that don't opt in. Otherwise:
 *
 * 1. Scans @p managed_path via @c dyad_cache_scan_dir(). If total usage
 *    is already at or below capacity, returns immediately without
 *    taking any lock.
 * 2. Acquires an exclusive lock on
 *    @c managed_path/DYAD_CACHE_LOCK_FILENAME (reusing @c dyad_excl_flock()),
 *    then re-scans and re-checks usage in case another process already
 *    evicted enough while this one was waiting for the lock.
 * 3. Sorts candidates by recency key ascending (oldest/least-recently
 *    used first per the active policy) and @c unlink()s them, skipping
 *    any candidate that is currently locked by another process (via
 *    @c dyad_try_excl_flock()) or was accessed within
 *    @c ctx->cache_grace_period_sec, until usage drops to
 *    @c ctx->cache_low_watermark_frac * ctx->cache_capacity_bytes.
 * 4. Releases the directory lock.
 *
 * Eviction failures on individual files are logged and skipped -- this
 * function never turns a successful @c dyad_produce()/@c dyad_consume()
 * call into a reported error.
 *
 * @param[in] ctx          DYAD context.
 * @param[in] managed_path Managed directory to enforce the capacity of
 *                         (@c ctx->prod_managed_path or @c ctx->cons_managed_path).
 * @return @c DYAD_RC_OK always (best-effort; see above).
 */
dyad_rc_t dyad_cache_maybe_evict (dyad_ctx_t *ctx, const char *managed_path);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_CACHE_DYAD_CACHE_INT_H */
