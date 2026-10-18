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

dyad_rc_t dyad_cache_scan_dir (const dyad_ctx_t *ctx,
                               const char *managed_path,
                               struct dyad_cache_entry **out_entries,
                               size_t *out_n_entries,
                               uint64_t *out_total_size)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DIR *dirp = NULL;
    struct dirent *dent = NULL;
    struct dyad_cache_entry *entries = NULL;
    size_t n_entries = 0;
    size_t capacity = 0;
    uint64_t total_size = 0;

    *out_entries = NULL;
    *out_n_entries = 0;
    *out_total_size = 0;

    dirp = opendir (managed_path);
    if (dirp == NULL) {
        DYAD_LOG_ERROR (ctx,
                        "DYAD CACHE: Cannot open managed directory %s for scanning",
                        managed_path);
        return DYAD_RC_BADFIO;
    }

    while ((dent = readdir (dirp)) != NULL) {
        char path[PATH_MAX + 1];
        struct stat st;
        int64_t recency_key = 0;

        if (strcmp (dent->d_name, ".") == 0 || strcmp (dent->d_name, "..") == 0
            || strcmp (dent->d_name, DYAD_CACHE_LOCK_FILENAME) == 0
            || dyad_cache_is_range_bitmap (dent->d_name)) {
            continue;
        }
        snprintf (path, sizeof (path), "%s/%s", managed_path, dent->d_name);
        if (stat (path, &st) != 0 || !S_ISREG (st.st_mode)) {
            continue;
        }
        if (ctx->cache_policy == NULL || ctx->cache_policy->get_recency_key == NULL
            || DYAD_IS_ERROR (ctx->cache_policy->get_recency_key (ctx, &st, &recency_key))) {
            // No usable policy -- skip this candidate rather than guessing an order.
            continue;
        }

        if (n_entries == capacity) {
            size_t new_capacity = (capacity == 0) ? 16 : capacity * 2;
            struct dyad_cache_entry *new_entries =
                realloc (entries, new_capacity * sizeof (struct dyad_cache_entry));
            if (new_entries == NULL) {
                DYAD_LOG_ERROR (ctx,
                                "DYAD CACHE: Cannot allocate memory while scanning %s",
                                managed_path);
                free (entries);
                closedir (dirp);
                return DYAD_RC_SYSFAIL;
            }
            entries = new_entries;
            capacity = new_capacity;
        }

        strncpy (entries[n_entries].path, path, PATH_MAX);
        entries[n_entries].path[PATH_MAX] = '\0';
        entries[n_entries].size = st.st_size;
        entries[n_entries].atime = st.st_atime;
        entries[n_entries].recency_key = recency_key;
        total_size += (uint64_t)st.st_size;
        n_entries++;
    }
    closedir (dirp);

    *out_entries = entries;
    *out_n_entries = n_entries;
    *out_total_size = total_size;
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
    // reusing the same dyad_excl_flock() primitive used for per-file
    // coordination elsewhere in DYAD.
    snprintf (lock_path, sizeof (lock_path), "%s/%s", managed_path, DYAD_CACHE_LOCK_FILENAME);
    lock_fd = open (lock_path, O_RDWR | O_CREAT, 0666);
    if (lock_fd == -1) {
        DYAD_LOG_ERROR (ctx, "DYAD CACHE: Cannot open cache lock file %s", lock_path);
        return DYAD_RC_OK;
    }
    if (DYAD_IS_ERROR (dyad_excl_flock (ctx, lock_fd, &dir_lock))) {
        close (lock_fd);
        return DYAD_RC_OK;
    }

    // Re-check: another process may have already evicted while we waited.
    if (DYAD_IS_ERROR (
            dyad_cache_scan_dir (ctx, managed_path, &entries, &n_entries, &total_size))) {
        dyad_release_flock (ctx, lock_fd, &dir_lock);
        close (lock_fd);
        return DYAD_RC_OK;
    }
    if (total_size <= ctx->cache_capacity_bytes) {
        free (entries);
        dyad_release_flock (ctx, lock_fd, &dir_lock);
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
        if (dyad_try_excl_flock (ctx, victim_fd, &victim_lock) != DYAD_RC_OK) {
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
        dyad_release_flock (ctx, victim_fd, &victim_lock);
        close (victim_fd);
    }

    free (entries);
    dyad_release_flock (ctx, lock_fd, &dir_lock);
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
