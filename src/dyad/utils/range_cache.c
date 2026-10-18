#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

// clang-format off
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// clang-format on

#if defined(__cplusplus)
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#else
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <dyad/utils/range_cache.h>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/utils/utils.h>

#define DYAD_RANGE_CACHE_SUFFIX ".dyad_cached"

// Cache OFD locks (dyad_cache_shared_flock()/dyad_cache_excl_flock()) are
// associated with each descriptor, so they exclude both threads and
// processes. This mutex additionally protects this process's bitmap
// read-modify-write sequence without holding a kernel lock across the slow
// origin fetch. It is held in addition to the OFD locks, which
// remain necessary for the cross-process case (e.g. this module's worker
// threads racing a local consumer process reading the same shard directly,
// see dyad_client.c's dyad_consume_range()). A single global mutex is fine
// here since the guarded work is a few KB of pread()/bit-check/pwrite() on
// the bitmap file (microseconds) -- the slow origin fetch always happens
// with neither lock held (see the "no lock held" comment on
// range_cache_fetch_span() below).
static pthread_mutex_t g_range_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Builds the path of a range-cache bitmap sidecar file.
 *
 * @param[in]  local_path   Path of the locally cached data file.
 * @param[out] out          Buffer that receives the sidecar path.
 * @param[in]  out_capacity Capacity of @p out in bytes.
 *
 * @return @c DYAD_RC_OK on success, or @c DYAD_RC_BADBUF if @p out is too
 *         small for the path and suffix.
 */
static dyad_rc_t range_cache_bitmap_path (const char *local_path, char *out, size_t out_capacity)
{
    if (snprintf (out, out_capacity, "%s%s", local_path, DYAD_RANGE_CACHE_SUFFIX)
        >= (int)out_capacity) {
        return DYAD_RC_BADBUF;
    }
    return DYAD_RC_OK;
}

/**
 * @brief Calculates the number of cache blocks needed for a file.
 *
 * @param[in] file_size Size of the file in bytes.
 *
 * @return Number of @c DYAD_RANGE_CACHE_BLOCK_SIZE blocks that cover
 *         @p file_size bytes.
 */
static inline size_t range_cache_num_blocks (size_t file_size)
{
    return (file_size + DYAD_RANGE_CACHE_BLOCK_SIZE - 1ul) / DYAD_RANGE_CACHE_BLOCK_SIZE;
}

/**
 * @brief Calculates the bitmap size needed to track a number of blocks.
 *
 * @param[in] num_blocks Number of blocks to track.
 *
 * @return Number of bytes required for one bit per block.
 */
static inline size_t range_cache_bitmap_bytes (size_t num_blocks)
{
    return (num_blocks + 7ul) / 8ul;
}

/**
 * @brief Checks whether every block in an inclusive range is cached.
 *
 * @param[in] bitmap      Bitmap with one bit per cache block.
 * @param[in] block_start First block in the inclusive range.
 * @param[in] block_end   Last block in the inclusive range.
 *
 * @return @c true if all blocks in the range are set; otherwise @c false.
 */
static bool range_cache_blocks_set (const uint8_t *bitmap, size_t block_start, size_t block_end)
{
    size_t b = 0ul;
    for (b = block_start; b <= block_end; b++) {
        if ((bitmap[b / 8ul] & (uint8_t)(1u << (b % 8ul))) == 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Marks every block in an inclusive range as cached.
 *
 * @param[in,out] bitmap      Bitmap with one bit per cache block.
 * @param[in]     block_start First block in the inclusive range.
 * @param[in]     block_end   Last block in the inclusive range.
 *
 * @return None.
 */
static void range_cache_set_blocks (uint8_t *bitmap, size_t block_start, size_t block_end)
{
    size_t b = 0ul;
    for (b = block_start; b <= block_end; b++) {
        bitmap[b / 8ul] |= (uint8_t)(1u << (b % 8ul));
    }
}

/**
 * @brief Copies an inclusive block range from the origin to the local cache.
 *
 * The copy is performed with no bitmap lock held. Concurrent callers may
 * duplicate an overlapping copy, but write identical bytes at identical
 * offsets and therefore do not corrupt the local cache.
 *
 * @param[in] ctx         DYAD context used for logging.
 * @param[in] local_path  Destination path of the local cache file.
 * @param[in] origin_path Source path of the authoritative file.
 * @param[in] block_start First block in the inclusive range to copy.
 * @param[in] block_end   Last block in the inclusive range to copy.
 * @param[in] origin_size Size of the source file in bytes; caps the final
 *                        copied block at end of file.
 *
 * @return @c DYAD_RC_OK on success, @c DYAD_RC_BADFIO for an I/O failure, or
 *         @c DYAD_RC_SYSFAIL if allocation of the transfer buffer fails.
 */
static dyad_rc_t range_cache_fetch_span (const dyad_ctx_t *ctx,
                                         const char *local_path,
                                         const char *origin_path,
                                         size_t block_start,
                                         size_t block_end,
                                         size_t origin_size)
{
    dyad_rc_t rc = DYAD_RC_OK;
    int origin_fd = -1;
    int local_fd = -1;
    struct flock local_lock;
    bool local_locked = false;
    void *span_buf = NULL;
    size_t span_off = block_start * DYAD_RANGE_CACHE_BLOCK_SIZE;
    size_t span_end = (block_end + 1ul) * DYAD_RANGE_CACHE_BLOCK_SIZE;
    size_t span_len = 0ul;

    if (span_end > origin_size) {
        span_end = origin_size;
    }
    if (span_end <= span_off) {
        return DYAD_RC_BADFIO;
    }
    span_len = span_end - span_off;

    span_buf = malloc (span_len);
    if (span_buf == NULL) {
        return DYAD_RC_SYSFAIL;
    }

    origin_fd = open (origin_path, O_RDONLY);
    if (origin_fd == -1
        || pread (origin_fd, span_buf, span_len, (off_t)span_off) != (ssize_t)span_len) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot read span [%zu, %zu) from origin '%s'",
                               span_off,
                               span_end,
                               origin_path);
        rc = DYAD_RC_BADFIO;
        goto fetch_span_done;
    }

    local_fd = open (local_path, O_WRONLY);
    if (local_fd == -1 || DYAD_IS_ERROR (dyad_cache_excl_flock (ctx, local_fd, &local_lock))) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot lock local '%s'", local_path);
        rc = DYAD_RC_BADFIO;
        goto fetch_span_done;
    }
    local_locked = true;
    if (pwrite (local_fd, span_buf, span_len, (off_t)span_off) != (ssize_t)span_len) {
        DYAD_WORKER_LOG_ERROR (
            "DYAD RANGE_CACHE: cannot write span [%zu, %zu) into local '%s'",
            span_off,
            span_end,
            local_path);
        rc = DYAD_RC_BADFIO;
        goto fetch_span_done;
    }
    rc = DYAD_RC_OK;

fetch_span_done:;
    if (origin_fd != -1) {
        close (origin_fd);
    }
    if (local_fd != -1) {
        if (local_locked) {
            dyad_cache_release_flock (ctx, local_fd, &local_lock);
        }
        close (local_fd);
    }
    free (span_buf);
    return rc;
}

dyad_rc_t dyad_range_cache_ensure (const dyad_ctx_t *ctx,
                                   const char *local_path,
                                   const char *origin_path,
                                   size_t offset,
                                   size_t length)
{
    dyad_rc_t rc = DYAD_RC_OK;
    char bitmap_path[PATH_MAX + 1] = {'\0'};
    int bitmap_fd = -1;
    int size_fd = -1;
    struct flock lock;
    struct stat origin_st;
    uint8_t *bitmap = NULL;
    size_t bitmap_bytes = 0ul;
    size_t num_blocks = 0ul;
    size_t block_start = 0ul;
    size_t block_end = 0ul;
    size_t origin_size = 0ul;
    size_t request_end = 0ul;
    bool already_cached = false;

    if (origin_path == NULL || length == 0ul) {
        return DYAD_RC_OK;
    }

    rc = range_cache_bitmap_path (local_path, bitmap_path, sizeof (bitmap_path));
    if (DYAD_IS_ERROR (rc)) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: local path '%s' is too long", local_path);
        return rc;
    }

    if (offset > SIZE_MAX - length) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: range overflows for origin file '%s'", origin_path);
        return DYAD_RC_BADFIO;
    }
    block_start = offset / DYAD_RANGE_CACHE_BLOCK_SIZE;
    request_end = offset + length;
    block_end = (request_end - 1ul) / DYAD_RANGE_CACHE_BLOCK_SIZE;

    bitmap_fd = open (bitmap_path, O_RDWR | O_CREAT, 0644);
    if (bitmap_fd == -1) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot open bitmap file '%s'", bitmap_path);
        return DYAD_RC_BADFIO;
    }

    // g_range_cache_mutex guards against races between worker threads of
    // *this* process (fcntl() below only excludes other processes -- see
    // the comment on g_range_cache_mutex's declaration). Held continuously
    // across the fast-path check and the metadata-init+recheck below (both
    // read-modify-check the bitmap), then released before the slow origin
    // fetch, then re-acquired just to record the fetched span -- mirroring
    // the existing fcntl() shared/exclusive escalation dance below, just
    // without a shared/exclusive distinction (pthread_mutex_t has none).
    pthread_mutex_lock (&g_range_cache_mutex);

    // Fast path: under a shared lock, check whether the requested span is
    // already fully cached.
    rc = dyad_cache_shared_flock (ctx, bitmap_fd, &lock);
    if (DYAD_IS_ERROR (rc)) {
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return rc;
    }
    bitmap_bytes = (size_t)get_file_size (bitmap_fd);
    if (bitmap_bytes > block_end / 8ul) {
        bitmap = (uint8_t *)malloc (bitmap_bytes);
        if (bitmap != NULL && pread (bitmap_fd, bitmap, bitmap_bytes, 0) == (ssize_t)bitmap_bytes
            && range_cache_blocks_set (bitmap, block_start, block_end)) {
            free (bitmap);
            dyad_cache_release_flock (ctx, bitmap_fd, &lock);
            pthread_mutex_unlock (&g_range_cache_mutex);
            close (bitmap_fd);
            return DYAD_RC_OK;
        }
        free (bitmap);
        bitmap = NULL;
    }
    dyad_cache_release_flock (ctx, bitmap_fd, &lock);

    // Miss (or bitmap not yet sized): escalate to an exclusive lock just
    // long enough to do the cheap metadata work (stat/ftruncate/re-check),
    // then release it *before* doing the slow origin fetch + local write --
    // holding it across that would serialize every concurrent miss on this
    // shard (even for disjoint spans) behind whatever process got there
    // first, which is exactly what turned occasional slow PFS reads into
    // multi-second-to-tens-of-seconds stalls for every other consumer of
    // the same shard queued behind the lock.
    rc = dyad_cache_excl_flock (ctx, bitmap_fd, &lock);
    if (DYAD_IS_ERROR (rc)) {
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return rc;
    }

    if (stat (origin_path, &origin_st) != 0 || origin_st.st_size <= 0) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot stat origin file '%s'", origin_path);
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return DYAD_RC_BADFIO;
    }
    origin_size = (size_t)origin_st.st_size;
    if (offset >= origin_size) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: invalid range for origin file '%s'", origin_path);
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return DYAD_RC_BADFIO;
    }
    if (request_end > origin_size) {
        request_end = origin_size;
    }
    block_end = (request_end - 1ul) / DYAD_RANGE_CACHE_BLOCK_SIZE;
    num_blocks = range_cache_num_blocks (origin_size);
    bitmap_bytes = range_cache_bitmap_bytes (num_blocks);

    if ((size_t)get_file_size (bitmap_fd) < bitmap_bytes) {
        // First touch for this local_path: size both the bitmap and the
        // (possibly not-yet-existing) local data file once, up front.
        size_fd = open (local_path, O_WRONLY | O_CREAT, 0644);
        if (size_fd == -1 || ftruncate (size_fd, (off_t)origin_size) != 0
            || ftruncate (bitmap_fd, (off_t)bitmap_bytes) != 0) {
            DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot size local file/bitmap for '%s'",
                                   local_path);
            if (size_fd != -1) {
                close (size_fd);
            }
            dyad_cache_release_flock (ctx, bitmap_fd, &lock);
            pthread_mutex_unlock (&g_range_cache_mutex);
            close (bitmap_fd);
            return DYAD_RC_BADFIO;
        }
        close (size_fd);
    }

    bitmap = (uint8_t *)calloc (1ul, bitmap_bytes);
    if (bitmap == NULL) {
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return DYAD_RC_SYSFAIL;
    }
    // A short/zero read here just means no blocks are set yet (e.g. right
    // after the ftruncate() above), which range_cache_blocks_set() below
    // will correctly treat as a miss since bitmap was calloc'd to all-zero.
    pread (bitmap_fd, bitmap, bitmap_bytes, 0);
    already_cached = range_cache_blocks_set (bitmap, block_start, block_end);
    free (bitmap);
    bitmap = NULL;
    dyad_cache_release_flock (ctx, bitmap_fd, &lock);
    pthread_mutex_unlock (&g_range_cache_mutex);

    if (already_cached) {
        // Another process (or thread) filled this span while we waited.
        close (bitmap_fd);
        return DYAD_RC_OK;
    }

    // Fetch from origin_path into local_path with *no* lock held (neither
    // fcntl() nor g_range_cache_mutex). If another process/thread is
    // concurrently missing on an overlapping span (same shard), both will
    // redundantly pread the same origin bytes and pwrite them to the same
    // local_path offset -- since both write identical data to the same
    // range, this races safely (worst case is duplicated I/O, never
    // corruption), and is a better trade than serializing unrelated misses
    // behind a single slow PFS read.
    rc = range_cache_fetch_span (ctx, local_path, origin_path, block_start, block_end, origin_size);
    if (DYAD_IS_ERROR (rc)) {
        close (bitmap_fd);
        return rc;
    }

    // Re-acquire both locks just long enough to record that this span is
    // now cached, merging with whatever bits any concurrent racers have
    // set.
    pthread_mutex_lock (&g_range_cache_mutex);
    rc = dyad_cache_excl_flock (ctx, bitmap_fd, &lock);
    if (DYAD_IS_ERROR (rc)) {
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return rc;
    }
    bitmap = (uint8_t *)calloc (1ul, bitmap_bytes);
    if (bitmap == NULL) {
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
        pthread_mutex_unlock (&g_range_cache_mutex);
        close (bitmap_fd);
        return DYAD_RC_SYSFAIL;
    }
    pread (bitmap_fd, bitmap, bitmap_bytes, 0);
    range_cache_set_blocks (bitmap, block_start, block_end);
    if (pwrite (bitmap_fd, bitmap, bitmap_bytes, 0) != (ssize_t)bitmap_bytes) {
        DYAD_WORKER_LOG_ERROR ("DYAD RANGE_CACHE: cannot update bitmap file '%s'", bitmap_path);
        rc = DYAD_RC_BADFIO;
    }
    free (bitmap);
    dyad_cache_release_flock (ctx, bitmap_fd, &lock);
    pthread_mutex_unlock (&g_range_cache_mutex);
    close (bitmap_fd);
    return rc;
}

dyad_rc_t dyad_range_cache_is_complete (const dyad_ctx_t *ctx,
                                        const char *local_path,
                                        bool *has_range_cache,
                                        bool *complete)
{
    char bitmap_path[PATH_MAX + 1] = {'\0'};
    struct stat data_st;
    struct flock lock;
    uint8_t *bitmap = NULL;
    size_t num_blocks = 0ul;
    size_t bitmap_bytes = 0ul;
    int bitmap_fd = -1;
    dyad_rc_t rc = DYAD_RC_OK;

    if (has_range_cache == NULL || complete == NULL) {
        return DYAD_RC_BADBUF;
    }
    *has_range_cache = false;
    *complete = true;
    rc = range_cache_bitmap_path (local_path, bitmap_path, sizeof (bitmap_path));
    if (DYAD_IS_ERROR (rc)) {
        return rc;
    }
    bitmap_fd = open (bitmap_path, O_RDONLY);
    if (bitmap_fd < 0) {
        return errno == ENOENT ? DYAD_RC_OK : DYAD_RC_BADFIO;
    }
    *has_range_cache = true;
    if (stat (local_path, &data_st) != 0 || data_st.st_size <= 0) {
        *complete = false;
        close (bitmap_fd);
        return DYAD_RC_OK;
    }
    num_blocks = range_cache_num_blocks ((size_t)data_st.st_size);
    bitmap_bytes = range_cache_bitmap_bytes (num_blocks);
    bitmap = malloc (bitmap_bytes);
    if (bitmap == NULL) {
        close (bitmap_fd);
        return DYAD_RC_SYSFAIL;
    }
    pthread_mutex_lock (&g_range_cache_mutex);
    rc = dyad_cache_shared_flock (ctx, bitmap_fd, &lock);
    if (!DYAD_IS_ERROR (rc)
        && (get_file_size (bitmap_fd) < (ssize_t)bitmap_bytes
            || pread (bitmap_fd, bitmap, bitmap_bytes, 0) != (ssize_t)bitmap_bytes
            || !range_cache_blocks_set (bitmap, 0ul, num_blocks - 1ul))) {
        *complete = false;
    }
    if (!DYAD_IS_ERROR (rc)) {
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
    }
    pthread_mutex_unlock (&g_range_cache_mutex);
    free (bitmap);
    close (bitmap_fd);
    return rc;
}

dyad_rc_t dyad_range_cache_mark_complete (const dyad_ctx_t *ctx, const char *local_path)
{
    char bitmap_path[PATH_MAX + 1] = {'\0'};
    struct stat data_st;
    struct flock lock;
    uint8_t *bitmap = NULL;
    size_t num_blocks = 0ul;
    size_t bitmap_bytes = 0ul;
    int bitmap_fd = -1;
    dyad_rc_t rc = DYAD_RC_OK;
    bool locked = false;

    rc = range_cache_bitmap_path (local_path, bitmap_path, sizeof (bitmap_path));
    if (DYAD_IS_ERROR (rc)) {
        return rc;
    }
    bitmap_fd = open (bitmap_path, O_RDWR);
    if (bitmap_fd < 0) {
        return errno == ENOENT ? DYAD_RC_OK : DYAD_RC_BADFIO;
    }
    if (stat (local_path, &data_st) != 0 || data_st.st_size <= 0) {
        close (bitmap_fd);
        return DYAD_RC_BADFIO;
    }
    num_blocks = range_cache_num_blocks ((size_t)data_st.st_size);
    bitmap_bytes = range_cache_bitmap_bytes (num_blocks);
    bitmap = malloc (bitmap_bytes);
    if (bitmap == NULL) {
        close (bitmap_fd);
        return DYAD_RC_SYSFAIL;
    }
    memset (bitmap, 0xff, bitmap_bytes);
    pthread_mutex_lock (&g_range_cache_mutex);
    rc = dyad_cache_excl_flock (ctx, bitmap_fd, &lock);
    locked = !DYAD_IS_ERROR (rc);
    if (!DYAD_IS_ERROR (rc)
        && (ftruncate (bitmap_fd, (off_t)bitmap_bytes) != 0
            || pwrite (bitmap_fd, bitmap, bitmap_bytes, 0) != (ssize_t)bitmap_bytes)) {
        rc = DYAD_RC_BADFIO;
    }
    if (locked) {
        dyad_cache_release_flock (ctx, bitmap_fd, &lock);
    }
    pthread_mutex_unlock (&g_range_cache_mutex);
    free (bitmap);
    close (bitmap_fd);
    return rc;
}
