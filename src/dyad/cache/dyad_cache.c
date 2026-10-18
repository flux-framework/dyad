#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dyad/cache/dyad_cache_int.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/utils/utils.h>
// clang-format on

// POSIX specifies st_blocks in units of 512 bytes, independent of the
// filesystem's allocation-block size.
#define DYAD_STAT_BLOCK_BYTES 512ULL

/**
 * @brief Returns whether a directory entry is a range-cache bitmap sidecar.
 *
 * @param[in] name Directory-entry name, not a full path.
 *
 * @return @c true when @p name ends in @c .dyad_cached; otherwise @c false.
 */
static bool dyad_cache_is_range_bitmap (const char *name)
{
    static const char suffix[] = ".dyad_cached";
    const size_t name_len = strlen (name);
    const size_t suffix_len = sizeof (suffix) - 1ul;
    return name_len > suffix_len && strcmp (name + name_len - suffix_len, suffix) == 0;
}

/**
 * @brief Appends regular files in one managed-directory subtree to an eviction list.
 *
 * @param[in]     ctx         DYAD context containing the active cache policy.
 * @param[in]     directory   Directory to scan recursively.
 * @param[in,out] entries     Candidate array, reallocated as needed.
 * @param[in,out] n_entries   Number of populated entries in @p entries.
 * @param[in,out] capacity    Allocated entry capacity.
 * @param[in,out] total_size  Sum of allocated bytes of all appended entries.
 *
 * @retval DYAD_RC_OK      All entries in @p directory were scanned.
 * @retval DYAD_RC_BADFIO The directory could not be opened.
 * @retval DYAD_RC_SYSFAIL Candidate-array allocation failed.
 */
static dyad_rc_t dyad_cache_scan_subtree (const dyad_ctx_t *DYAD_RESTRICT ctx,
                                          const char *DYAD_RESTRICT directory,
                                          struct dyad_cache_entry **DYAD_RESTRICT entries,
                                          size_t *DYAD_RESTRICT n_entries,
                                          size_t *DYAD_RESTRICT capacity,
                                          uint64_t *DYAD_RESTRICT total_size)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DIR *dirp = NULL;
    struct dirent *dent = NULL;

    dirp = opendir (directory);
    if (dirp == NULL) {
        DYAD_LOG_ERROR (ctx,
                        "DYAD CACHE: Cannot open managed directory %s for scanning",
                        directory);
        return DYAD_RC_BADFIO;
    }

    while ((dent = readdir (dirp)) != NULL) {
        char path[PATH_MAX + 1];
        struct stat lst;
        struct stat st;
        int64_t recency_key = 0;
        int path_len = 0;

        if (strcmp (dent->d_name, ".") == 0 || strcmp (dent->d_name, "..") == 0
            || strcmp (dent->d_name, DYAD_CACHE_LOCK_FILENAME) == 0
            || dyad_cache_is_range_bitmap (dent->d_name)) {
            continue;
        }
        path_len = snprintf (path, sizeof (path), "%s/%s", directory, dent->d_name);
        if (path_len < 0 || (size_t)path_len >= sizeof (path)) {
            continue;
        }
        if (lstat (path, &lst) != 0) {
            continue;
        }
        if (S_ISDIR (lst.st_mode)) {
            rc = dyad_cache_scan_subtree (ctx, path, entries, n_entries, capacity, total_size);
            if (DYAD_IS_ERROR (rc)) {
                closedir (dirp);
                return rc;
            }
            continue;
        }
        if (stat (path, &st) != 0 || !S_ISREG (st.st_mode)) {
            continue;
        }
        if (ctx->cache_policy == NULL || ctx->cache_policy->get_recency_key == NULL
            || DYAD_IS_ERROR (ctx->cache_policy->get_recency_key (ctx, &st, &recency_key))) {
            // No usable policy -- skip this candidate rather than guessing an order.
            continue;
        }

        if (*n_entries == *capacity) {
            size_t new_capacity = (*capacity == 0) ? 16 : *capacity * 2;
            struct dyad_cache_entry *new_entries =
                realloc (*entries, new_capacity * sizeof (struct dyad_cache_entry));
            if (new_entries == NULL) {
                DYAD_LOG_ERROR (ctx,
                                "DYAD CACHE: Cannot allocate memory while scanning %s",
                                directory);
                closedir (dirp);
                return DYAD_RC_SYSFAIL;
            }
            *entries = new_entries;
            *capacity = new_capacity;
        }

        memcpy ((*entries)[*n_entries].path, path, (size_t)path_len + 1ul);
        // st_size is a sparse file's logical length. Cache capacity is disk
        // capacity, so account for only filesystem blocks actually allocated.
        (*entries)[*n_entries].size = (off_t)((uint64_t)st.st_blocks * DYAD_STAT_BLOCK_BYTES);
        (*entries)[*n_entries].atime = st.st_atime;
        (*entries)[*n_entries].recency_key = recency_key;
        *total_size += (uint64_t)(*entries)[*n_entries].size;
        (*n_entries)++;
    }
    closedir (dirp);

    return rc;
}

dyad_rc_t dyad_cache_scan_dir (const dyad_ctx_t *ctx,
                               const char *managed_path,
                               struct dyad_cache_entry **out_entries,
                               size_t *out_n_entries,
                               uint64_t *out_total_size)
{
    size_t capacity = 0;
    dyad_rc_t rc = DYAD_RC_OK;

    *out_entries = NULL;
    *out_n_entries = 0;
    *out_total_size = 0;

    rc = dyad_cache_scan_subtree (
        ctx, managed_path, out_entries, out_n_entries, &capacity, out_total_size);
    if (DYAD_IS_ERROR (rc)) {
        free (*out_entries);
        *out_entries = NULL;
        *out_n_entries = 0;
        *out_total_size = 0;
    }
    return rc;
}

static int dyad_cache_entry_cmp (const void *a, const void *b)
{
    const struct dyad_cache_entry *ea = (const struct dyad_cache_entry *)a;
    const struct dyad_cache_entry *eb = (const struct dyad_cache_entry *)b;
    if (ea->recency_key < eb->recency_key)
        return -1;
    if (ea->recency_key > eb->recency_key)
        return 1;
    return 0;
}

dyad_rc_t dyad_cache_maybe_evict (dyad_ctx_t *ctx, const char *managed_path)
{
    struct dyad_cache_entry *entries = NULL;
    size_t n_entries = 0;
    uint64_t total_size = 0;
    uint64_t low_watermark = 0;
    char lock_path[PATH_MAX + 1];
    int lock_fd = -1;
    struct flock dir_lock;
    time_t now = 0;
    size_t i = 0;
    size_t n_evicted = 0;
    uint64_t bytes_freed = 0;

    // Zero-overhead when disabled (the default) -- a single check, no
    // filesystem access at all.
    if (ctx == NULL || ctx->cache_capacity_bytes == 0 || ctx->cache_policy_mode == DYAD_CACHE_NONE
        || managed_path == NULL) {
        return DYAD_RC_OK;
    }

    if (DYAD_IS_ERROR (
            dyad_cache_scan_dir (ctx, managed_path, &entries, &n_entries, &total_size))) {
        // Best-effort: a scan failure should not fail the caller's produce/consume.
        return DYAD_RC_OK;
    }
    if (total_size <= ctx->cache_capacity_bytes) {
        free (entries);
        return DYAD_RC_OK;
    }
    free (entries);
    entries = NULL;

    // Over capacity: serialize the scan-and-evict decision across
    // processes sharing this managed directory via a dedicated lock file,
    // using cache-specific OFD locks so concurrent worker threads in this
    // process are serialized as well.
    snprintf (lock_path, sizeof (lock_path), "%s/%s", managed_path, DYAD_CACHE_LOCK_FILENAME);
    lock_fd = open (lock_path, O_RDWR | O_CREAT, 0666);
    if (lock_fd == -1) {
        DYAD_LOG_ERROR (ctx, "DYAD CACHE: Cannot open cache lock file %s", lock_path);
        return DYAD_RC_OK;
    }
    if (DYAD_IS_ERROR (dyad_cache_excl_flock (ctx, lock_fd, &dir_lock))) {
        close (lock_fd);
        return DYAD_RC_OK;
    }

    // Re-check: another process may have already evicted while we waited.
    if (DYAD_IS_ERROR (
            dyad_cache_scan_dir (ctx, managed_path, &entries, &n_entries, &total_size))) {
        dyad_cache_release_flock (ctx, lock_fd, &dir_lock);
        close (lock_fd);
        return DYAD_RC_OK;
    }
    if (total_size <= ctx->cache_capacity_bytes) {
        free (entries);
        dyad_cache_release_flock (ctx, lock_fd, &dir_lock);
        close (lock_fd);
        return DYAD_RC_OK;
    }

    low_watermark = (uint64_t)(ctx->cache_low_watermark_frac * (double)ctx->cache_capacity_bytes);
    qsort (entries, n_entries, sizeof (struct dyad_cache_entry), dyad_cache_entry_cmp);
    now = time (NULL);

    for (i = 0; i < n_entries && total_size > low_watermark; i++) {
        int victim_fd = -1;
        struct flock victim_lock;

        if ((now - entries[i].atime) < (time_t)ctx->cache_grace_period_sec) {
            // Recently accessed -- may still be mid-access. Skip.
            continue;
        }

        victim_fd = open (entries[i].path, O_RDWR);
        if (victim_fd == -1) {
            // Already gone (e.g. raced with another evictor or the owner).
            continue;
        }
        if (dyad_cache_try_excl_flock (ctx, victim_fd, &victim_lock) != DYAD_RC_OK) {
            // Locked by an in-flight dyad_produce()/dyad_consume() elsewhere. Skip.
            close (victim_fd);
            continue;
        }
        if (unlink (entries[i].path) == 0) {
            char bitmap_path[PATH_MAX + 1];

            // A range-cache bitmap describes only this data file.  Remove it
            // with the victim so a later cache miss cannot trust stale bits.
            if (snprintf (bitmap_path, sizeof (bitmap_path), "%s.dyad_cached", entries[i].path)
                < (int)sizeof (bitmap_path)) {
                unlink (bitmap_path);
            }
            total_size -= (uint64_t)entries[i].size;
            bytes_freed += (uint64_t)entries[i].size;
            n_evicted++;
        }
        dyad_cache_release_flock (ctx, victim_fd, &victim_lock);
        close (victim_fd);
    }

    free (entries);
    dyad_cache_release_flock (ctx, lock_fd, &dir_lock);
    close (lock_fd);

    DYAD_LOG_INFO (ctx,
                   "DYAD CACHE: [node %u rank %u pid %d] Evicted %zu file(s), freed %llu bytes "
                   "from %s",
                   ctx->node_idx,
                   ctx->rank,
                   ctx->pid,
                   n_evicted,
                   (unsigned long long)bytes_freed,
                   managed_path);

    return DYAD_RC_OK;
}
