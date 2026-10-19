#ifndef DYAD_CORE_DYAD_CORE_H
#define DYAD_CORE_DYAD_CORE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>
#include <dyad/core/dyad_ctx.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if DYAD_PERFFLOW
#define DYAD_CORE_FUNC_MODS __attribute__ ((annotate ("@critical_path()"))) static
#else
#define DYAD_CORE_FUNC_MODS static inline
#endif

struct dyad_metadata {
    char *fpath;
    uint32_t owner_rank;
};
typedef struct dyad_metadata dyad_metadata_t;

/**
 * @brief Publishes a file under a DYAD-managed directory so it is available to consumers.
 *
 * @details
 * If @p fname falls under the producer-managed path, this function publishes
 * the file's metadata to the Flux KVS via @c dyad_commit(), signaling to
 * consumers that the file is ready to be read.
 *
 * This function is the producer-side counterpart to @c dyad_consume(). It
 * does not transfer file data directly; instead it notifies the DYAD
 * infrastructure that the file has been written, allowing waiting consumers
 * to proceed with retrieval.
 *
 * @param[in]     ctx    Pointer to the DYAD context. Must not be @c NULL and must
 *                       have a valid @c prod_managed_path set.
 * @param[in]     fname  Path to the file that has been written and is ready for
 *                       consumption.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK              The file was successfully published.
 * @retval DYAD_RC_NOCTX           The context @p ctx or its Flux handle is @c NULL.
 * @retval DYAD_RC_BADMANAGEDPATH  The producer-managed path in the context is @c NULL.
 * @retval DYAD_RC_*               Any error code propagated from @c dyad_commit().
 *
 * @note The caller is responsible for ensuring the file has been fully written
 *       and flushed to storage before calling this function, as consumers may
 *       begin reading the file immediately upon notification.
 *
 * @warning The caller must ensure @p ctx remains valid for the duration of this call.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_produce (dyad_ctx_t *ctx, const char *fname);

/**
 * @brief Retrieves metadata for a file under a DYAD-managed directory.
 *
 * @details
 * Resolves @p fname to a path relative to the consumer-managed directory and
 * populates @p mdata with the file's metadata. This is used by the Python API
 * and is intended to be paired with @c dyad_consume_w_metadata(), which uses
 * the populated metadata to perform the actual data transfer.
 *
 * If the file already exists locally, metadata is constructed directly from
 * the local file without consulting the Flux KVS. Otherwise, the metadata is
 * looked up from the KVS, optionally blocking until the producer publishes it.
 *
 * Unlike @c dyad_consume(), files outside the consumer-managed path are not
 * silently ignored — @c DYAD_RC_UNTRACKED is returned instead, allowing the
 * caller to distinguish between a managed file that is not yet available and
 * a file that DYAD is not responsible for.
 *
 * On error, any partially allocated @p mdata is freed before returning.
 *
 * @param[in]     ctx          Pointer to the DYAD context. Must not be @c NULL.
 * @param[in]     fname        Path to the file whose metadata is to be retrieved.
 *                             Must not be @c NULL or empty. May be an absolute path
 *                             or, if @c ctx->relative_to_managed_path is set, a path
 *                             relative to @c ctx->cons_managed_path.
 * @param[in]     should_wait  If @c true, block until the producer publishes the
 *                             file's metadata to the KVS. If @c false, return
 *                             immediately if the metadata is not yet available.
 * @param[out]    mdata        Address of a @c dyad_metadata_t pointer to be populated.
 *                             Must not be @c NULL. If @c *mdata is already allocated,
 *                             it is reused; otherwise a new object is allocated.
 *                             The caller is responsible for freeing it via
 *                             @c dyad_free_metadata() when no longer needed.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK        Metadata was successfully retrieved.
 * @retval DYAD_RC_UNTRACKED @p fname is not under the consumer-managed path.
 * @retval DYAD_RC_NOTFOUND  The file exists locally but @p mdata is @c NULL.
 * @retval DYAD_RC_BADFIO    @p fname is an empty string.
 * @retval DYAD_RC_SYSFAIL   Memory allocation for the metadata object failed.
 * @retval DYAD_RC_*         Any error code propagated from @c dyad_kvs_read().
 *
 * @note This function temporarily sets @c ctx->reenter to @c false during
 *       execution to prevent re-entrant interception, restoring it to @c true
 *       before returning.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_get_metadata (dyad_ctx_t *ctx,
                                                                 const char *fname,
                                                                 bool should_wait,
                                                                 dyad_metadata_t **mdata);

/**
 * @brief Frees a @c dyad_metadata_t object allocated by @c dyad_get_metadata().
 *
 * @details
 * Releases all memory associated with @p mdata, including the internal file
 * path string, and sets @c *mdata to @c NULL. If @p mdata or @c *mdata is
 * @c NULL, the function returns @c DYAD_RC_OK without taking any action.
 *
 * @param[in]     mdata  Address of the @c dyad_metadata_t pointer to free.
 *                       Set to @c NULL on return.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK  The metadata was successfully freed, or was already @c NULL.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_free_metadata (dyad_metadata_t **mdata);

/**
 * @brief Ensures a file under a DYAD-managed directory is ready to be read.
 *
 * @details
 * If @p fname falls under the consumer-managed path, this function ensures the
 * file is fully available before the caller proceeds to read it.
 *
 * If @p fname is not under the consumer-managed path, the function returns
 * @c DYAD_RC_OK immediately without taking any action.
 *
 * The behavior depends on the underlying storage configuration:
 *
 * - **Shared storage** (e.g., a parallel or network filesystem visible across
 *   multiple nodes): The file is visible to all consumers if it exists. An
 *   exclusive lock is acquired to synchronize access. A file size of zero
 *   indicates the producer has not yet written the file, so the lock is
 *   released and the function waits for the file to be published via the
 *   Flux KVS before returning. No data transfer is performed since the file
 *   is directly accessible once available.
 *
 * - **Node-local storage**: The file being empty means it is either not yet
 *   produced or not yet visible locally. While holding the exclusive lock, the
 *   function waits for the file to be published via the Flux KVS. Since the
 *   producer does not participate in locking, it may write the file during
 *   this wait. When metadata is returned by @c dyad_fetch_metadata(), if the
 *   file has local visibility (e.g., the producer is on the same node and has
 *   already written the file), @c dyad_fetch_metadata() frees and nulls the
 *   metadata object to signal that no remote transfer is needed. Otherwise,
 *   the file data is retrieved from the remote producer's Flux broker via
 *   @c dyad_get_data(), written to local disk, and the lock is released.
 *
 *  * An exclusive OFD lock is acquired to serve two purposes. First, the producer
 * acquires its conflicting POSIX lock in @c dyad_open_wrapper() to prevent consumers with
 * direct file visibility (e.g. via shared storage or co-location on the
 * same node) from reading a partially written file, and releases it in
 * @c dyad_close_wrapper() once the file is fully written. Second, the
 * consumer acquires the lock here to ensure that only one consumer performs
 * the data fetch at a time, with other consumers blocking until the lock is
 * released. Because POSIX @c fcntl locks are cooperative, these guarantees
 * only hold between processes that also participate in locking.
 *
 * @param[in]     ctx    Pointer to the DYAD context. Must not be @c NULL and must
 *                       have a valid @c cons_managed_path set.
 * @param[in]     fname  Path to the file to be checked and made ready. May be an
 *                       absolute path or, if @c ctx->relative_to_managed_path is
 *                       set, a path relative to @c ctx->cons_managed_path.
 * @param[out]    is_managed  Set to @c true when @p fname is under the
 *                             consumer-managed path; otherwise set to @c false.
 *                             Must not be @c NULL.
 * @param[out]    guard_fd    Optional descriptor holding a shared OFD lock
 *                             across the caller's real open. Set to @c -1 when
 *                             no guard is transferred. The caller must close a
 *                             transferred descriptor after locking its reader
 *                             descriptor, including if the real open fails.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK              The file is ready to read, was already available,
 *                                 or is not under the managed path (no action needed).
 * @retval DYAD_RC_NOCTX           The context @p ctx or its Flux handle is @c NULL.
 * @retval DYAD_RC_BADMANAGEDPATH  The consumer-managed path in the context is @c NULL.
 * @retval DYAD_RC_BADFIO          A file I/O error occurred (open or close failed).
 * @retval DYAD_RC_*               Any error code propagated from @c dyad_excl_flock(),
 *                                 @c dyad_fetch_metadata(), @c dyad_get_data(), or
 *                                 @c dyad_cons_store().
 *
 * @note This function temporarily sets @c ctx->reenter to @c false during execution
 *       to prevent re-entrant interception, restoring it to @c true before returning.
 *
 * @warning The caller must ensure @p ctx remains valid for the duration of this call.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_consume (dyad_ctx_t *ctx,
                                                            const char *fname,
                                                            bool *is_managed,
                                                            int *guard_fd);

/**
 * @brief Ensures a file is ready to be read using caller-supplied metadata.
 *
 * @details
 * This is a variant of @c dyad_consume() intended for use by the Python API,
 * where metadata is managed manually by the caller. Instead of resolving the
 * file path and consulting the Flux KVS itself, this function accepts a
 * pre-populated @c dyad_metadata_t object obtained from a prior call to
 * @c dyad_get_metadata().
 *
 * If @p mdata is @c NULL, the file is assumed to be already available locally
 * and the function returns @c DYAD_RC_OK without taking any action. Otherwise,
 * if the file is empty (not yet fetched), the function retrieves the file data
 * from the producer's Flux broker via @c dyad_get_data() and writes it to
 * local disk via @c dyad_cons_store().
 *
 * As with @c dyad_consume(), an exclusive lock is acquired for consumer-to-consumer
 * synchronization. Because OFD and POSIX @c fcntl locks are cooperative, the producer —
 * which does not acquire any lock — is unaffected and may write the file
 * regardless of any consumer lock held.
 *
 * The typical usage pattern from Python is:
 *  1. Call @c dyad_get_metadata() to obtain @p mdata, optionally waiting for
 *     the producer to publish the file.
 *  2. Pass @p mdata to this function to ensure the file is locally available.
 *  3. Read the file.
 *  4. Free @p mdata via @c dyad_free_metadata().
 *
 * @param[in]     ctx    Pointer to the DYAD context. Must not be @c NULL and must
 *                       have a valid @c cons_managed_path set.
 * @param[in]     fname  Path to the file to be checked and made ready. Must not
 *                       be @c NULL.
 * @param[in]     mdata  Metadata for the file, previously obtained via
 *                       @c dyad_get_metadata() or manually constructed by the
 *                       caller. If @c NULL, the file is assumed to be locally
 *                       available and no transfer is performed. The caller retains
 *                       ownership and is responsible for freeing this object after
 *                       this function returns.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK              The file is ready to read, or @p mdata was
 *                                 @c NULL indicating the file is already local.
 * @retval DYAD_RC_NOCTX           The context @p ctx or its Flux handle is @c NULL.
 * @retval DYAD_RC_BADMANAGEDPATH  The consumer-managed path in the context is @c NULL.
 * @retval DYAD_RC_BADFIO          A file I/O error occurred (open or close failed).
 * @retval DYAD_RC_*               Any error code propagated from @c dyad_excl_flock(),
 *                                 @c dyad_get_data(), or @c dyad_cons_store().
 *
 * @note This function temporarily sets @c ctx->reenter to @c false during execution
 *       to prevent re-entrant interception, restoring it to @c true before returning.
 *
 * @warning The caller must ensure @p ctx remains valid for the duration of this call.
 * @warning Unlike @c dyad_consume(), this function does not support shared storage;
 *          it always attempts to fetch and store the file locally when the file
 *          is empty and @p mdata is non-@c NULL.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t
dyad_consume_w_metadata (dyad_ctx_t *ctx, const char *fname, const dyad_metadata_t *mdata);

/**
 * @brief Fetches a byte range of a file without materializing a local copy.
 *
 * @details
 * Unlike @c dyad_consume()/@c dyad_consume_w_metadata(), which always
 * materialize a complete local file (required for GOTCHA @c open()
 * interception compatibility), this function returns the requested
 * @p [offset, offset + length) range of @p fname directly to the caller as
 * an in-memory buffer -- no local file is created or written. Intended for
 * callers (e.g. the pydyad HDF/flat-cache binding) that only need raw bytes,
 * not a file descriptor.
 *
 * The file must already have been published via @c dyad_produce() by some
 * rank (its full contents staged there beforehand -- this function does not
 * stage or copy files, only fetches a sub-range of one that's already fully
 * present somewhere). Behavior:
 *  1. Resolves @p fname to a path relative to the consumer-managed
 *     directory, same as @c dyad_consume().
 *  2. Calls the existing @c dyad_fetch_metadata() unchanged: if the file is
 *     already present on this node (KVS lookup returns @c NULL), reads
 *     @p [offset, length) directly from the local copy via @c pread() --
 *     no RPC. Otherwise, issues a byte-range RPC to the owning rank
 *     returned in the metadata.
 *  3. Only implemented for @c DYAD_DTL_FLUX_RPC and @c DYAD_DTL_MARGO --
 *     returns @c DYAD_RC_BADDTLMODE immediately under @c DYAD_DTL_UCX.
 *
 * @param[in]  ctx      Pointer to the DYAD context. Must not be @c NULL.
 * @param[in]  fname    Path to the file to fetch from. Must not be @c NULL.
 * @param[in]  offset   Starting byte offset of the requested range.
 * @param[in]  length   Number of bytes requested.
 * @param[out] data     Set to a newly allocated buffer containing the
 *                       requested bytes. The caller must @c free() it.
 * @param[out] data_len Set to the number of bytes actually returned in
 *                       @p data (normally equal to @p length).
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK           The requested range was retrieved successfully.
 * @retval DYAD_RC_NOCTX        The context @p ctx or its Flux handle is @c NULL.
 * @retval DYAD_RC_BADMANAGEDPATH The consumer-managed path in the context is @c NULL.
 * @retval DYAD_RC_BADDTLMODE   The active DTL mode is @c DYAD_DTL_UCX, which
 *                              does not implement byte-range fetch.
 * @retval DYAD_RC_BADFIO       A local @c pread() failed.
 * @retval DYAD_RC_*            Any error code propagated from
 *                              @c dyad_fetch_metadata() or the internal
 *                              RPC/DTL path.
 */
DYAD_PFA_ANNOTATE DYAD_DLL_EXPORTED dyad_rc_t dyad_consume_range (dyad_ctx_t *ctx,
                                                                  const char *fname,
                                                                  size_t offset,
                                                                  size_t length,
                                                                  void **data,
                                                                  size_t *data_len);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_CORE_DYAD_CORE */
