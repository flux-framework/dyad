#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/cache/dyad_cache_int.h>
#include <dyad/client/dyad_client_int.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/utils/murmur3.h>
#include <dyad/utils/range_cache.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>
#include <flux/core.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
// clang-format on

#ifdef __cplusplus
#include <climits>
#include <cstring>
#else
#include <limits.h>
#include <linux/limits.h>
#include <string.h>
#endif

/**
 * @brief Generates a hierarchical KVS key from a file path using MurmurHash3.
 *
 * @details
 * Produces a structured KVS key of the form @c "h1.h2...hN.str", where each
 * @c hN is a hexadecimal hash bucket value and @c str is the original file path.
 * The number of hash levels is controlled by @p depth and the number of buckets
 * per level by @p width.
 *
 * Each level is computed by applying MurmurHash3_x64_128 to @p str with a
 * different seed (drawn from a fixed table of primes, cycling every 10 levels),
 * then reducing the four 32-bit hash words via XOR and taking the result modulo
 * @p width to select a bucket. The buckets from all levels are concatenated with
 * @c '.' separators, followed by the original @p str appended as the final
 * component.
 *
 * If @p str is shorter than 128 bytes, it is padded with @c '@' characters to
 * 128 bytes before hashing to improve hash distribution for short paths.
 *
 * The hierarchical structure distributes keys across a tree of KVS directories,
 * avoiding hot spots that would occur if all keys were stored at the same level.
 * @p depth and @p width together control the fanout and depth of this tree.
 *
 * @param[in]  str       File path to encode as a KVS key. Must not be @c NULL
 *                       or empty.
 * @param[out] path_key  Buffer to receive the generated key. Must not be @c NULL.
 *                       On success, contains a null-terminated string of the form
 *                       @c "h1.h2...hN.str".
 * @param[in]  len       Size of the @p path_key buffer in bytes. Must be greater
 *                       than zero and large enough to hold the full key including
 *                       the null terminator.
 * @param[in]  depth     Number of hash levels (directory levels) in the key.
 *                       Must be no greater than 10 to stay within the seed table;
 *                       larger values cycle through the seed table.
 * @param[in]  width     Number of buckets per hash level. Controls the fanout at
 *                       each directory level of the KVS key tree.
 *
 * @return int
 * @retval  0  The key was successfully generated and written to @p path_key.
 * @retval -1  @p str, @p path_key, or @p len is invalid, @p str is empty, or
 *             the generated key exceeded @p len.
 */
DYAD_DLL_EXPORTED int gen_path_key (const char *restrict str,
                                    char *restrict path_key,
                                    const size_t len,
                                    const uint32_t depth,
                                    const uint32_t width)
{
    DYAD_C_FUNCTION_START ();
    static const uint32_t seeds[10] =
        {104677u, 104681u, 104683u, 104693u, 104701u, 104707u, 104711u, 104717u, 104723u, 104729u};

    uint32_t seed = 57u;
    uint32_t hash[4] = {0u};  // Output for the hash
    char buf[256] = {'\0'};
    size_t cx = 0ul;
    int n = 0;
    const char *str_long = str;
    size_t str_len = 0ul;

    if (str == NULL || path_key == NULL || len == 0ul) {
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    str_len = strlen (str);
    if (str_len == 0ul) {
        DYAD_C_FUNCTION_END ();
        return -1;
    }

    path_key[0] = '\0';

    // Just append the string so that it can be as large as 128 bytes.
    if (str_len < 128ul) {
        memcpy (buf, str, str_len);
        memset (buf + str_len, '@', 128ul - str_len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    for (uint32_t d = 0u; d < depth; d++) {
        seed += seeds[d % 10];
        MurmurHash3_x64_128 (str_long, str_len, seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key + cx, len - cx, "%x.", bin);
        cx += n;
        if (cx >= len || n < 0) {
            DYAD_C_FUNCTION_END ();
            return -1;
        }
    }
    n = snprintf (path_key + cx, len - cx, "%s", str);
    if (n < 0) {
        DYAD_C_FUNCTION_END ();
        return -1;
    }
    DYAD_C_FUNCTION_UPDATE_STR ("path_key", path_key);
    DYAD_C_FUNCTION_END ();
    return 0;
}

/**
 * @brief Callback to clean up a Flux future after an asynchronous KVS commit completes.
 *
 * @details
 * Registered via @c flux_future_then() by @c dyad_kvs_commit() when
 * @c ctx->async_publish is enabled. Invoked by the Flux reactor when the
 * asynchronous KVS commit future is fulfilled, allowing the commit to complete
 * without blocking the caller.
 *
 * If the future completed with an error, logs a message to stderr before
 * destroying the future. The error is not propagated since there is no caller
 * context to return to at callback invocation time.
 *
 * @param[in] f    Pointer to the fulfilled Flux future. Destroyed before returning.
 * @param[in] arg  Unused. Reserved for future use.
 */
static void future_cleanup_cb (flux_future_t *f, void *arg)
{
    if (flux_future_get (f, NULL) < 0) {
        DYAD_LOG_STDERR ("future_cleanup: future error detected with.%s", "");
    }
    flux_future_destroy (f);
}

/**
 * @brief Commits a Flux KVS transaction to publish file metadata.
 *
 * @details
 * Submits @p txn to the Flux KVS under @c ctx->kvs_namespace. The commit
 * behavior depends on whether asynchronous publishing is enabled:
 *
 * - **Synchronous** (@c ctx->async_publish is @c false): Blocks until the
 *   commit is acknowledged by the KVS, then destroys the future. The caller
 *   can be certain the metadata is visible to consumers upon return.
 *
 * - **Asynchronous** (@c ctx->async_publish is @c true): Registers
 *   @c future_cleanup_cb via @c flux_future_then() and returns immediately
 *   without waiting for the commit to complete. The future is destroyed by
 *   the callback when the commit eventually completes. The caller cannot
 *   assume the metadata is visible to consumers upon return.
 *
 * This function is an internal helper called by @c publish_via_flux() and
 * is not intended to be called directly by users.
 *
 * @param[in] ctx  Pointer to the DYAD context. Must not be @c NULL. Provides
 *                 the Flux handle, KVS namespace, and @c async_publish flag.
 * @param[in] txn  Pointer to the Flux KVS transaction to commit. Must not be
 *                 @c NULL. The caller retains ownership and is responsible for
 *                 destroying @p txn after this function returns.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK        The transaction was successfully submitted, and in
 *                           synchronous mode, acknowledged by the KVS.
 * @retval DYAD_RC_BADCOMMIT The @c flux_kvs_commit() call failed to submit
 *                           the transaction.
 *
 * @note In asynchronous mode, a failure to register @c future_cleanup_cb via
 *       @c flux_future_then() is logged but does not affect the return code.
 *       The commit may still complete, but the future may not be properly
 *       cleaned up.
 */
DYAD_CORE_FUNC_MODS dyad_rc_t dyad_kvs_commit (const dyad_ctx_t *restrict ctx,
                                               flux_kvs_txn_t *restrict txn)
{
    DYAD_C_FUNCTION_START ();
    flux_future_t *f = NULL;
    dyad_rc_t rc = DYAD_RC_OK;
    // Commit the transaction to the Flux KVS
    f = flux_kvs_commit ((flux_t *)ctx->h, ctx->kvs_namespace, 0, txn);
    // If the commit failed, log an error and return DYAD_BADCOMMIT
    if (f == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not commit transaction to Flux KVS");
        rc = DYAD_RC_BADCOMMIT;
        goto kvs_commit_region_finish;
    }
    if (ctx->async_publish) {
        if (flux_future_then (f, -1, future_cleanup_cb, NULL) < 0) {
            DYAD_LOG_ERROR (ctx, "Error with flux_future_then");
        }
    } else {
        // If the commit is pending, wait for it to complete
        flux_future_wait_for (f, -1.0);
        // Once the commit is complete, destroy the future and transaction
        flux_future_destroy (f);
        f = NULL;
    }
    rc = DYAD_RC_OK;
kvs_commit_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Builds and commits a Flux KVS transaction to advertise a produced file.
 *
 * @details
 * Generates a KVS key from @p upath via @c gen_path_key(), then creates and
 * commits a single-entry Flux KVS transaction that maps the key to the
 * producer's broker rank (@c ctx->rank). This rank is later retrieved by
 * consumers via @c dyad_kvs_read() to determine file locality and, if needed,
 * to identify which broker to contact for data transfer.
 *
 * This function sits between @c dyad_commit() and @c dyad_kvs_commit() in the
 * producer publish pipeline:
 *
 * @code
 * dyad_produce()
 *     -> dyad_commit()          [resolves path, checks management, guards reenter]
 *         -> publish_via_flux() [builds and submits the KVS transaction]
 *             -> dyad_kvs_commit() [commits the transaction to the Flux KVS]
 * @endcode
 *
 * The KVS transaction is destroyed before returning regardless of whether the
 * commit succeeded or failed.
 *
 * This function is an internal helper and is not intended to be called directly
 * by users.
 *
 * @param[in] ctx    Pointer to the DYAD context. Must not be @c NULL. Provides
 *                   the Flux handle, KVS namespace, producer rank, and key
 *                   generation parameters (@c key_depth and @c key_bins).
 * @param[in] upath  Path to the file relative to the producer-managed directory.
 *                   Used to generate the KVS key. Must not be @c NULL.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK        The transaction was successfully built and committed.
 * @retval DYAD_RC_FLUXFAIL  The Flux KVS transaction could not be created or packed.
 * @retval DYAD_RC_*         Any error code propagated from @c dyad_kvs_commit().
 */
DYAD_CORE_FUNC_MODS dyad_rc_t publish_via_flux (const dyad_ctx_t *restrict ctx,
                                                const char *restrict upath)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", ctx->fname);
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    dyad_rc_t rc = DYAD_RC_OK;
    flux_kvs_txn_t *txn = NULL;
    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1] = {'\0'};
    memset (topic, 0, topic_len + 1);
    memset (topic, '\0', topic_len + 1);
    // Generate the KVS key from the file path relative to
    // the producer-managed directory
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Generating KVS key from path (%s)", upath);
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    // Crete and pack a Flux KVS transaction.
    // The transaction will contain a single key-value pair
    // with the previously generated key as the key and the
    // producer's rank as the value
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Creating FLUX KVS transaction under the key %s", topic);
    txn = flux_kvs_txn_create ();
    if (txn == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not create Flux KVS transaction");
        rc = DYAD_RC_FLUXFAIL;
        goto publish_done;
    }
    if (flux_kvs_txn_pack (txn, 0, topic, "i", ctx->rank) < 0) {
        DYAD_LOG_ERROR (ctx, "Could not pack Flux KVS transaction");
        rc = DYAD_RC_FLUXFAIL;
        goto publish_done;
    }
    // Call dyad_kvs_commit to commit the transaction into the Flux KVS
    rc = dyad_kvs_commit (ctx, txn);
    // If dyad_kvs_commit failed, log an error and forward the return code
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "dyad_kvs_commit failed!");
        goto publish_done;
    }
    rc = DYAD_RC_OK;
publish_done:;
    if (txn != NULL) {
        flux_kvs_txn_destroy (txn);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Publishes file metadata to the Flux KVS to notify consumers that a file is ready.
 *
 * @details
 * Resolves @p fname to a path relative to the producer-managed directory and
 * publishes the file's metadata to the Flux KVS via @c publish_via_flux(). This
 * signals to waiting consumers that the file has been written and is ready to
 * be read or transferred.
 *
 * If @p fname is not under the producer-managed path, the function returns
 * @c DYAD_RC_OK immediately without taking any action, so that DYAD does not
 * interfere with file operations outside its managed directories.
 *
 * This function is the internal implementation called by @c dyad_produce(). It
 * may also be called directly when finer control over the commit step is needed,
 * such as when bypassing the context validation performed by @c dyad_produce().
 *
 * @param[in]     ctx    Pointer to the DYAD context. Must not be @c NULL and must
 *                       have a valid @c prod_managed_path set. @c ctx->reenter is
 *                       temporarily set to @c false during the KVS publish to
 *                       prevent re-entrant interception.
 * @param[in]     fname  Path to the file to be published. May be an absolute path
 *                       or, if @c ctx->relative_to_managed_path is set, a path
 *                       relative to @c ctx->prod_managed_path.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK   The file metadata was successfully published, or @p fname
 *                      is not under the producer-managed path (no action taken).
 * @retval DYAD_RC_*    Any error code propagated from @c publish_via_flux().
 *
 * @note The caller is responsible for ensuring the file has been fully written
 *       and flushed to storage before calling this function, as consumers may
 *       begin reading the file immediately upon receiving the KVS notification.
 * @note If @c ctx->check is set and the operation succeeds, the environment
 *       variable @c DYAD_CHECK_ENV is set to @c "ok".
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_commit (dyad_ctx_t *restrict ctx, const char *restrict fname)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", ctx->fname);
    dyad_rc_t rc = DYAD_RC_OK;
    char upath[PATH_MAX + 1] = {'\0'};
#if 0
    if (fname == NULL || strlen (fname) > PATH_MAX) {
        rc = DYAD_RC_SYSFAIL;
        goto get_metadata_done;
    }
#endif
    // As this is a function called for DYAD producer, ctx->prod_managed_path
    // must be a valid string (!NULL). ctx->delim_len is verified to be greater
    // than 0 during initialization.
    if (ctx->relative_to_managed_path &&
        //(strlen (fname) > 0ul) && // checked where get_path() was
        (strncmp (fname, DYAD_PATH_DELIM, ctx->delim_len)
         != 0)) {  // fname is a relative path that is relative to the
                   // prod_managed_path
        memcpy (upath, fname, strlen (fname));
    } else if (!cmp_canonical_path_prefix (ctx, true, fname, upath, PATH_MAX)) {
        // Extract the path to the file specified by fname relative to the
        // producer-managed path
        // This relative path will be stored in upath
        DYAD_LOG_DEBUG (ctx, "%s is not in the Producer's managed path", fname);
        rc = DYAD_RC_OK;
        goto commit_done;
    }
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_LOG_INFO (ctx, "DYAD CLIENT: commit file: %s", upath);
    // Call publish_via_flux to actually store information about the file into
    // the Flux KVS
    // Fence this call with reassignments of reenter so that, if intercepting
    // file I/O API calls, we will not get stuck in infinite recursion
    ctx->reenter = false;
    rc = publish_via_flux (ctx, upath);
    ctx->reenter = true;

commit_done:;
    // If "check" is set and the operation was successful, set the
    // DYAD_CHECK_ENV environment variable to "ok"
    if (rc == DYAD_RC_OK && (ctx && ctx->check)) {
        setenv (DYAD_CHECK_ENV, "ok", 1);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

static void print_mdata (const dyad_ctx_t *restrict ctx, const dyad_metadata_t *restrict mdata)
{
    if (mdata == NULL) {
        DYAD_LOG_INFO (ctx, "Cannot print a NULL metadata object!");
    } else {
        DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Printing contents of DYAD Metadata object");
        DYAD_LOG_DEBUG (ctx, "               fpath = %s", mdata->fpath);
        DYAD_LOG_DEBUG (ctx, "               owner_rank = %u", mdata->owner_rank);
    }
}

/**
 * @brief Looks up file metadata from the Flux KVS.
 *
 * @details
 * Queries the Flux KVS for metadata associated with the file identified by
 * @p topic (the KVS key) and @p upath (the file path relative to the
 * consumer-managed directory). The only metadata currently stored in the KVS
 * is the producer's broker rank (@c owner_rank), which is used by the caller
 * to determine locality and, if needed, to dispatch an RPC to the correct
 * producer broker for data transfer.
 *
 * If @p should_wait is @c true, the lookup blocks using @c FLUX_KVS_WAITCREATE
 * until the producer publishes the metadata. If @p should_wait is @c false,
 * the lookup returns immediately with @c DYAD_RC_NOTFOUND if the metadata is
 * not yet available.
 *
 * If @c *mdata is already allocated on entry, the existing object is reused
 * and only @c fpath and @c owner_rank are overwritten. Otherwise a new
 * @c dyad_metadata_t object is allocated. On error, any partially allocated
 * @p mdata is freed before returning.
 *
 * @param[in]     ctx         Pointer to the DYAD context. Must not be @c NULL.
 *                            Provides the Flux handle and KVS namespace.
 * @param[in]     topic       KVS key for the file, generated from @p upath via
 *                            @c gen_path_key(). Must not be @c NULL.
 * @param[in]     upath       Path to the file relative to the consumer-managed
 *                            directory. Stored in the populated metadata object.
 *                            Must not be @c NULL.
 * @param[in]     should_wait If @c true, block until the producer publishes the
 *                            metadata to the KVS. If @c false, return immediately
 *                            if the metadata is not yet available.
 * @param[in,out] mdata       Address of a @c dyad_metadata_t pointer to be
 *                            populated. Must not be @c NULL. If @c *mdata is
 *                            already allocated, it is reused; otherwise a new
 *                            object is allocated. The caller is responsible for
 *                            freeing it via @c dyad_free_metadata() when no
 *                            longer needed.
 *
 * @return @c dyad_rc_t         return code indicating the outcome:
 * @retval DYAD_RC_OK           Metadata was successfully retrieved and @p mdata
 *                              has been populated.
 * @retval DYAD_RC_NOTFOUND     @p mdata is @c NULL, the KVS lookup failed, or
 *                              the metadata is not yet available and
 *                              @p should_wait is @c false.
 * @retval DYAD_RC_SYSFAIL      Memory allocation for the metadata object or its
 *                              @c fpath field failed.
 * @retval DYAD_RC_BADMETADATA  The KVS response could not be unpacked to extract
 *                              the producer's broker rank.
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_kvs_read (const dyad_ctx_t *restrict ctx,
                                           const char *restrict topic,
                                           const char *restrict upath,
                                           bool should_wait,
                                           dyad_metadata_t **restrict mdata)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    dyad_rc_t rc = DYAD_RC_OK;
    int kvs_lookup_flags = 0;
    flux_future_t *f = NULL;
    if (mdata == NULL) {
        DYAD_LOG_ERROR (ctx,
                        "Metadata double pointer is NULL. "
                        "Cannot correctly create metadata object");
        rc = DYAD_RC_NOTFOUND;
        goto kvs_read_end;
    }
    // Lookup information about the desired file (represented by kvs_topic)
    // from the Flux KVS. If there is no information, wait for it to be
    // made available
    if (should_wait)
        kvs_lookup_flags = FLUX_KVS_WAITCREATE;
    f = flux_kvs_lookup ((flux_t *)ctx->h, ctx->kvs_namespace, kvs_lookup_flags, topic);
    // If the KVS lookup failed, log an error and return DYAD_BADLOOKUP
    if (f == NULL) {
        DYAD_LOG_ERROR (ctx, "KVS lookup failed!\n");
        rc = DYAD_RC_NOTFOUND;
        goto kvs_read_end;
    }
    // Extract the rank of the producer from the KVS response
    if (*mdata != NULL) {
        DYAD_LOG_INFO (ctx, "Metadata object is already allocated. Skipping allocation");
    } else {
        *mdata = (dyad_metadata_t *)malloc (sizeof (struct dyad_metadata));
        if (*mdata == NULL) {
            DYAD_LOG_ERROR (ctx, "Cannot allocate memory for metadata object");
            rc = DYAD_RC_SYSFAIL;
            goto kvs_read_end;
        }
    }
    size_t upath_len = strlen (upath);
    (*mdata)->fpath = (char *)malloc (upath_len + 1);
    if ((*mdata)->fpath == NULL) {
        DYAD_LOG_ERROR (ctx, "Cannot allocate memory for fpath in metadata object");
        rc = DYAD_RC_SYSFAIL;
        goto kvs_read_end;
    }
    memset ((*mdata)->fpath, '\0', upath_len + 1);
    memcpy ((*mdata)->fpath, upath, upath_len);
    rc = flux_kvs_lookup_get_unpack (f, "i", &((*mdata)->owner_rank));
    // If the extraction did not work, log an error and return DYAD_BADFETCH
    if (rc < 0) {
        DYAD_LOG_ERROR (ctx, "Could not unpack owner's rank from KVS response\n");
        rc = DYAD_RC_BADMETADATA;
        goto kvs_read_end;
    }
    DYAD_LOG_INFO (ctx, "DYAD CLIENT: Successfully retrieved metadata for key %s", topic);
    print_mdata (ctx, *mdata);
    DYAD_C_FUNCTION_UPDATE_STR ("fpath", (*mdata)->fpath);
    DYAD_C_FUNCTION_UPDATE_INT ("owner_rank", (*mdata)->owner_rank);
    rc = DYAD_RC_OK;

kvs_read_end:;
    if (DYAD_IS_ERROR (rc) && mdata != NULL && *mdata != NULL) {
        dyad_free_metadata (mdata);
    }
    if (f != NULL) {
        flux_future_destroy (f);
        f = NULL;
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Retrieves file metadata from the Flux KVS for an internal consumer operation.
 *
 * @details
 * Looks up metadata for the file identified by @p upath in the Flux KVS,
 * blocking until the producer publishes it. This is the internal counterpart
 * to @c dyad_get_metadata(), used exclusively by @c dyad_consume().
 *
 * After retrieving metadata, the function determines whether a remote data
 * transfer is actually needed by checking if the producer and consumer reside
 * on the same node. This is done by comparing @c mdata->owner_rank / @c ctx->service_mux
 * against @c ctx->node_idx. If they match, the producer is on the same node and
 * the file is already locally accessible, so the metadata object is freed and
 * set to @c NULL to signal to @c dyad_consume() that the data transfer step
 * should be skipped.
 *
 * Note that this locality check is only relevant for node-local storage. When
 * shared storage is enabled, @c dyad_consume() never uses the metadata for data
 * transfer regardless, so the check is not needed in that path.
 *
 * The @c mdata == @c NULL convention on return is therefore not an error condition
 * but a deliberate signal that the data transfer step should be skipped.
 *
 * This function is not intended to be called directly by users.
 *
 * @param[in]  ctx    Pointer to the DYAD context. Must not be @c NULL.
 * @param[in]  fname  Absolute path to the file being consumed.
 * @param[in]  upath  Path to the file relative to the consumer-managed directory.
 *                    Used to generate the Flux KVS lookup key.
 * @param[out] mdata  Address of a @c dyad_metadata_t pointer. Always set to
 *                    @c NULL on entry. On success, either points to the retrieved
 *                    metadata if a remote transfer is needed, or remains @c NULL
 *                    if the producer is on the same node and the file is already
 *                    locally accessible.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK   Metadata was successfully retrieved, or the producer is
 *                      on the same node and no transfer is needed (@p mdata will
 *                      be @c NULL).
 * @retval DYAD_RC_*    Any error code propagated from @c dyad_kvs_read().
 *
 * @note Unlike @c dyad_get_metadata(), this function always blocks until the
 *       producer publishes the file's metadata to the KVS.
 * @note The @c service_mux field in @p ctx controls how many Flux broker ranks
 *       map to a single node, and is used to determine node-level locality.
 */
DYAD_CORE_FUNC_MODS dyad_rc_t dyad_fetch_metadata (const dyad_ctx_t *restrict ctx,
                                                   const char *restrict fname,
                                                   const char *restrict upath,
                                                   dyad_metadata_t **restrict mdata)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", fname);
    dyad_rc_t rc = DYAD_RC_OK;
    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1] = {'\0'};
    *mdata = NULL;
#if 0
    if (fname == NULL || upath == NULL || strlen (fname) == 0ul || strlen (upath) == 0ul) {
        rc = DYAD_RC_BADFIO;
        goto get_metadata_done;
    }
#endif
    // Set reenter to false to avoid recursively performing DYAD operations
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    // Generate the KVS key from the file path relative to
    // the consumer-managed directory
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    DYAD_LOG_INFO (ctx, "DYAD CLIENT: Fetch metadata for: %s, key: %s.", upath, topic);
    // Call dyad_kvs_read to retrieve infromation about the file
    // from the Flux KVS
    rc = dyad_kvs_read (ctx, topic, upath, true, mdata);
    // If an error occured in dyad_kvs_read, log it and propagate the return
    // code
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: dyad_kvs_read failed!");
        goto fetch_done;
    }
    // There are two cases where we do not want to perform file transfer:
    //   1. if the shared storage feature is enabled
    //   2. if the producer and consumer share the same rank
    // In either of these cases, skip the creation of the dyad_kvs_response_t
    // object, and return DYAD_OK. This will cause the file transfer step to be
    // skipped
    DYAD_C_FUNCTION_UPDATE_INT ("owner_rank", (*mdata)->owner_rank);
    DYAD_C_FUNCTION_UPDATE_INT ("node_idx", ctx->node_idx);
    // if (ctx->shared_storage || ((*mdata)->owner_rank / ctx->service_mux) == ctx->node_idx) {
    if (((*mdata)->owner_rank / ctx->service_mux) == ctx->node_idx) {
        DYAD_LOG_INFO (ctx,
                       "Either shared-storage is indicated or the producer rank (%u) is the"
                       " same as the consumer rank (%u)",
                       (*mdata)->owner_rank,
                       ctx->rank);
        if (mdata != NULL && *mdata != NULL) {
            dyad_free_metadata (mdata);
        }
        rc = DYAD_RC_OK;
        DYAD_C_FUNCTION_UPDATE_INT ("is_local", 1);
        goto fetch_done;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("is_local", 0);
    rc = DYAD_RC_OK;

fetch_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Retrieves file data from a remote producer's Flux broker via RPC.
 *
 * @details
 * Dispatches a streaming Flux RPC to the DYAD module running on the producer's
 * broker (identified by @p mdata->owner_rank) and retrieves the file data via
 * the configured Data Transport Layer (DTL). The retrieved data is returned in
 * @p file_data and its length in @p file_len.
 *
 * The sequence of operations is:
 *  1. Pack an RPC payload containing the file path and producer rank.
 *  2. Send a streaming Flux RPC to the producer's DYAD module.
 *  3. Receive and parse the RPC response.
 *  4. Establish a DTL connection to the producer.
 *  5. Receive the file data over the DTL connection.
 *  6. Close the DTL connection.
 *  7. Wait for the end-of-stream RPC message from the producer module.
 *
 * The streaming RPC protocol expects exactly one data message followed by an
 * end-of-stream signal (indicated by @c ENODATA). If additional messages arrive
 * or the module reports an error, @c DYAD_RC_BADRPC is returned. Two return
 * codes from the DTL have special meaning and bypass the end-of-stream wait:
 * @c DYAD_RC_RPC_FINISHED (end of stream already received) and
 * @c DYAD_RC_BADRPC (a prior RPC operation failed irrecoverably).
 *
 * When built with UCX DTL support (@c DYAD_ENABLE_UCX_DTL), the producer
 * prepends the file size to the data buffer. This prefix is extracted and
 * stripped before returning, so @p file_data always points to the raw file
 * contents.
 *
 * This function is an internal helper called by @c dyad_consume() and
 * @c dyad_consume_w_metadata(). It is not intended to be called directly
 * by users.
 *
 * @param[in]  ctx        Pointer to the DYAD context. Must not be @c NULL.
 *                        Provides the Flux handle, DTL handle, and other
 *                        connection parameters.
 * @param[in]  mdata      Metadata for the file to retrieve. Must not be @c NULL.
 *                        @c mdata->fpath and @c mdata->owner_rank identify the
 *                        file and the producer broker to contact.
 * @param[out] file_data  Address of a pointer to be set to the buffer containing
 *                        the retrieved file data. The buffer is allocated by the
 *                        DTL layer. The caller is responsible for releasing it
 *                        via @c ctx->dtl_handle->return_buffer().
 * @param[out] file_len   Address of a @c size_t to be set to the number of bytes
 *                        in @p file_data.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK           File data was successfully retrieved.
 * @retval DYAD_RC_BADRPC       An RPC operation failed, the producer module sent
 *                              an unexpected number of responses, or the module
 *                              reported an error.
 * @retval DYAD_RC_BADFIO       UCX DTL only: the producer-prepended file size
 *                              was negative, indicating a read failure on the
 *                              producer side.
 * @retval DYAD_RC_*            Any error code propagated from
 *                              @c dtl_handle->rpc_pack(),
 *                              @c dtl_handle->rpc_recv_response(),
 *                              @c dtl_handle->establish_connection(), or
 *                              @c dtl_handle->recv().
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_get_data (const dyad_ctx_t *restrict ctx,
                                           const dyad_metadata_t *restrict mdata,
                                           char **restrict file_data,
                                           size_t *restrict file_len)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    flux_future_t *f = NULL;
    json_t *rpc_payload = NULL;
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Packing payload for RPC to DYAD module");
    DYAD_C_FUNCTION_UPDATE_INT ("owner_rank", mdata->owner_rank);
    DYAD_C_FUNCTION_UPDATE_STR ("fpath", mdata->fpath);
    rc = ctx->dtl_handle->rpc_pack (ctx, mdata->fpath, mdata->owner_rank, &rpc_payload);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx,
                        "Cannot create JSON payload for Flux RPC to "
                        "DYAD module\n");
        goto get_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Sending payload for RPC to DYAD module");
    f = flux_rpc_pack ((flux_t *)ctx->h,
                       DYAD_DTL_RPC_NAME,
                       mdata->owner_rank,
                       FLUX_RPC_STREAMING,
                       "o",
                       rpc_payload);
    if (f == NULL) {
        DYAD_LOG_ERROR (ctx, "Cannot send RPC to producer module.");
        rc = DYAD_RC_BADRPC;
        goto get_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Receive RPC response from DYAD module");
    rc = ctx->dtl_handle->rpc_recv_response (ctx, f);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot receive and/or parse the RPC response.");
        goto get_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Establish DTL connection with DYAD module");
    rc = ctx->dtl_handle->establish_connection (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx,
                        "Cannot establish connection with DYAD module on broker "
                        "%u.",
                        mdata->owner_rank);
        goto get_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Receive file data via DTL");
    rc = ctx->dtl_handle->recv (ctx, (void **)file_data, file_len);
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Close DTL connection with DYAD module");
    ctx->dtl_handle->close_connection (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot receive data from producer module.");
        goto get_done;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("file_len", *file_len);

    rc = DYAD_RC_OK;

get_done:;
    // There are two return codes that have special meaning when coming from the
    // DTL:
    //  * DYAD_RC_RPC_FINISHED: occurs when an ENODATA error occurs
    //  * DYAD_RC_BADRPC: occurs when a previous RPC operation fails
    // In either of these cases, we do not need to wait for the end of stream
    // because the RPC is already completely messed up. If we do not have either
    // of these cases, we will wait for one more RPC message. If everything went
    // well in the module, this last message will set errno to ENODATA (i.e.,
    // end of stream). Otherwise, something went wrong, so we'll return
    // DYAD_RC_BADRPC.
    // DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Wait for end-of-stream message from module (current RC =
    // %d)", rc);
    if (rc != DYAD_RC_RPC_FINISHED && rc != DYAD_RC_BADRPC) {
        if (!(flux_rpc_get (f, NULL) < 0 && errno == ENODATA)) {
            DYAD_LOG_ERROR (ctx,
                            "An error occured at end of getting data! Either the "
                            "module sent too many responses, or the module "
                            "failed with a bad error (errno = %d).",
                            errno);
            rc = DYAD_RC_BADRPC;
        }
    }
#ifdef DYAD_ENABLE_UCX_DTL
    // For UCX RMA, file_size was prepended to the buffer by the server; extract it
    // here to get the true data length and advance past the prefix.
    if (ctx->dtl_handle->mode == DYAD_DTL_UCX) {
        ctx->dtl_handle->get_buffer (ctx, 0, (void **)file_data);
        ssize_t read_len = 0l;
        memcpy (&read_len, *file_data, sizeof (read_len));
        if (read_len < 0l) {
            *file_len = 0ul;
            DYAD_LOG_DEBUG (ctx, "Not able to read from %s file", mdata->fpath);
            rc = DYAD_RC_BADFIO;
        } else {
            *file_len = (size_t)read_len;
        }
        *file_data = ((char *)*file_data) + sizeof (read_len);
    }
#endif
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Read %zd bytes from %s file", *file_len, mdata->fpath);
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Destroy the Flux future for the RPC.");
    flux_future_destroy (f);
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Retrieves a byte range of file data from a remote producer's Flux
 *        broker via RPC.
 *
 * @details
 * Parallel to @c dyad_get_data(), for @c dyad_consume_range() requests
 * (FLUX_RPC and MARGO DTL modes only, checked by the caller). Differs only
 * in: packs @p offset/@p length via @c rpc_pack_range() instead of just
 * @p mdata->fpath, and targets the @c DYAD_DTL_RPC_RANGE_NAME topic instead
 * of @c DYAD_DTL_RPC_NAME so it is routed to @c dyad_fetch_range_request_cb()
 * on the producer's broker.
 *
 * @param[in]  ctx        Pointer to the DYAD context. Must not be @c NULL.
 * @param[in]  mdata      Metadata for the file to retrieve. Must not be
 *                        @c NULL. @c mdata->fpath and @c mdata->owner_rank
 *                        identify the file and the producer broker to
 *                        contact.
 * @param[in]  offset     Starting byte offset of the requested range.
 * @param[in]  length     Number of bytes requested.
 * @param[out] file_data  Address of a pointer to be set to the buffer
 *                        containing the retrieved range. Allocated by the
 *                        DTL layer; the caller releases it via
 *                        @c ctx->dtl_handle->return_buffer().
 * @param[out] file_len   Address of a @c size_t to be set to the number of
 *                        bytes in @p file_data.
 *
 * @return @c dyad_rc_t return code, same conventions as @c dyad_get_data().
 */
DYAD_DLL_EXPORTED dyad_rc_t dyad_get_data_range (const dyad_ctx_t *restrict ctx,
                                                 const dyad_metadata_t *restrict mdata,
                                                 size_t offset,
                                                 size_t length,
                                                 char **restrict file_data,
                                                 size_t *restrict file_len)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    flux_future_t *f = NULL;
    json_t *rpc_payload = NULL;
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Packing ranged payload for RPC to DYAD module");
    DYAD_C_FUNCTION_UPDATE_INT ("owner_rank", mdata->owner_rank);
    DYAD_C_FUNCTION_UPDATE_STR ("fpath", mdata->fpath);
    rc = ctx->dtl_handle
             ->rpc_pack_range (ctx, mdata->fpath, mdata->owner_rank, offset, length, &rpc_payload);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx,
                        "Cannot create JSON payload for ranged Flux RPC to "
                        "DYAD module\n");
        goto get_range_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Sending ranged payload for RPC to DYAD module");
    f = flux_rpc_pack ((flux_t *)ctx->h,
                       DYAD_DTL_RPC_RANGE_NAME,
                       mdata->owner_rank,
                       FLUX_RPC_STREAMING,
                       "o",
                       rpc_payload);
    if (f == NULL) {
        DYAD_LOG_ERROR (ctx, "Cannot send ranged RPC to producer module.");
        rc = DYAD_RC_BADRPC;
        goto get_range_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Receive RPC response from DYAD module");
    rc = ctx->dtl_handle->rpc_recv_response (ctx, f);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot receive and/or parse the RPC response.");
        goto get_range_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Establish DTL connection with DYAD module");
    rc = ctx->dtl_handle->establish_connection (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx,
                        "Cannot establish connection with DYAD module on broker "
                        "%u.",
                        mdata->owner_rank);
        goto get_range_done;
    }
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Receive ranged file data via DTL");
    rc = ctx->dtl_handle->recv (ctx, (void **)file_data, file_len);
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Close DTL connection with DYAD module");
    ctx->dtl_handle->close_connection (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot receive ranged data from producer module.");
        goto get_range_done;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("file_len", *file_len);

    rc = DYAD_RC_OK;

get_range_done:;
    // Same end-of-stream handling as dyad_get_data() -- see its comment.
    if (rc != DYAD_RC_RPC_FINISHED && rc != DYAD_RC_BADRPC) {
        if (!(flux_rpc_get (f, NULL) < 0 && errno == ENODATA)) {
            DYAD_LOG_ERROR (ctx,
                            "An error occured at end of getting ranged data! Either the "
                            "module sent too many responses, or the module "
                            "failed with a bad error (errno = %d).",
                            errno);
            rc = DYAD_RC_BADRPC;
        }
    }
    DYAD_LOG_DEBUG (ctx,
                    "DYAD CLIENT: Read %zd bytes of range from %s file",
                    *file_len,
                    mdata->fpath);
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Destroy the Flux future for the RPC.");
    flux_future_destroy (f);
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Writes file data retrieved from a producer to the consumer-managed directory.
 *
 * @details
 * Stores @p data_len bytes of @p file_data to the appropriate path under the
 * consumer-managed directory, as determined by combining @c ctx->cons_managed_path
 * with the relative file path in @p mdata->fpath. Any intermediate directories
 * that do not yet exist are created as needed.
 *
 * For large files (at or above @c DYAD_POSIX_TRANSFER_GRANULARITY bytes), the
 * data is written in chunks of @c DYAD_POSIX_TRANSFER_GRANULARITY rather than
 * in a single @c write() call.
 *
 * This function is an internal helper called by @c dyad_consume() and
 * @c dyad_consume_w_metadata() after data has been retrieved from the producer
 * via @c dyad_get_data(). It is not intended to be called directly by users.
 *
 * @param[in] ctx        Pointer to the DYAD context. Must not be @c NULL. Used to
 *                       resolve the consumer-managed path and check the @c check flag.
 * @param[in] mdata      Metadata for the file being stored. Must not be @c NULL.
 *                       @c mdata->fpath is appended to @c ctx->cons_managed_path to
 *                       form the full destination path.
 * @param[in] fd         Open, writable file descriptor for the destination file.
 * @param[in] data_len   Number of bytes to write from @p file_data.
 * @param[in] file_data  Buffer containing the file data to write. Must be at least
 *                       @p data_len bytes in size.
 *
 * @return @c dyad_rc_t return code indicating the outcome:
 * @retval DYAD_RC_OK      All @p data_len bytes were successfully written.
 * @retval DYAD_RC_BADFIO  Directory creation failed, a @c write() call failed,
 *                         or the total bytes written does not match @p data_len.
 *
 * @note If the operation succeeds and @c ctx->check is set, the environment
 *       variable @c DYAD_CHECK_ENV is set to @c "ok".
 */
DYAD_CORE_FUNC_MODS dyad_rc_t dyad_cons_store (const dyad_ctx_t *restrict ctx,
                                               const dyad_metadata_t *restrict mdata,
                                               int fd,
                                               const size_t data_len,
                                               char *restrict file_data)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    dyad_rc_t rc = DYAD_RC_OK;
    const char *odir = NULL;
    char file_path[PATH_MAX + 1] = {'\0'};
    char file_path_copy[PATH_MAX + 1] = {'\0'};
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    size_t total_bytes_written = 0ul;
    const size_t granularity = (size_t)DYAD_POSIX_TRANSFER_GRANULARITY;
    size_t write_chunk_size = 0ul;
    ssize_t write_result = 0;
    int write_errno = 0;
    memset (file_path, 0, PATH_MAX + 1);
    memset (file_path_copy, 0, PATH_MAX + 1);

    // Build the full path to the file being consumed
    strncpy (file_path, ctx->cons_managed_path, PATH_MAX - 1);
    concat_str (file_path, mdata->fpath, "/", PATH_MAX);
    memcpy (file_path_copy, file_path, PATH_MAX);  // dirname modifies the arg
    DYAD_C_FUNCTION_UPDATE_STR ("cons_managed_path", ctx->cons_managed_path);
    DYAD_C_FUNCTION_UPDATE_STR ("fpath", mdata->fpath);
    DYAD_C_FUNCTION_UPDATE_STR ("file_path_copy", file_path_copy);

    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Saving retrieved data to %s", file_path);
    // Create the directory as needed
    // TODO: Need to be consistent with the mode at the source
    odir = dirname (file_path_copy);
    if ((strncmp (odir, ".", strlen (".")) != 0) && (mkdir_as_needed (odir, m) < 0)) {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: Cannot create needed directories for pulled file");
        rc = DYAD_RC_BADFIO;
        goto pull_done;
    }

    // Write the file contents to the location specified by the user
    if (data_len < granularity) {
        write_result = write (fd, file_data, data_len);
        if (write_result <= 0) {
            write_errno = errno;
            goto write_failed;
        }
        total_bytes_written = (size_t)write_result;
    } else {
        DYAD_LOG_DEBUG (ctx,
                        " writing file %s with bytes %zu is big. Writing in granularity %zu",
                        file_path,
                        data_len,
                        granularity);
        while (total_bytes_written < data_len) {
            write_chunk_size = ((data_len - total_bytes_written) > granularity)
                                   ? granularity
                                   : (data_len - total_bytes_written);
            write_result = write (fd, file_data + total_bytes_written, write_chunk_size);
            if (write_result <= 0) {
                write_errno = errno;
                goto write_failed;
            }
            DYAD_LOG_DEBUG (ctx,
                            " writing file %s with bytes %zu of %zd",
                            file_path,
                            write_chunk_size,
                            write_result);
            total_bytes_written += (size_t)write_result;
        }
    }
    if (total_bytes_written != data_len) {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: cons store write of pulled file failed!\n");
        rc = DYAD_RC_BADFIO;
        goto pull_done;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("data_len", data_len);
    rc = DYAD_RC_OK;
    goto pull_done;

write_failed:
    rc = DYAD_RC_BADFIO;
    if (write_result < 0) {
        DYAD_LOG_ERROR (ctx,
                        "DYAD CLIENT: Failed to write file \"%s\" with code %d:%s.",
                        file_path,
                        write_errno,
                        strerror (write_errno));
    } else {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: Write made no progress for file \"%s\".", file_path);
    }

pull_done:;
    // If "check" is set and the operation was successful, set the
    // DYAD_CHECK_ENV environment variable to "ok"
    if (rc == DYAD_RC_OK && (ctx && ctx->check))
        setenv (DYAD_CHECK_ENV, "ok", 1);
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_produce (dyad_ctx_t *restrict ctx, const char *restrict fname)
{
    DYAD_C_FUNCTION_START ();
    ctx->fname = fname;
    DYAD_C_FUNCTION_UPDATE_STR ("fname", ctx->fname);
    DYAD_LOG_DEBUG (ctx, "DYAD CLIENT: Executing dyad_produce");
    dyad_rc_t rc = DYAD_RC_OK;
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h) {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: No CTX found in dyad_produce");
        rc = DYAD_RC_NOCTX;
        goto produce_done;
    }
    // If the producer-managed path is NULL or empty, then the context is not
    // valid for a producer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->prod_managed_path == NULL) {
        DYAD_LOG_ERROR (ctx, "DYAD CLIENT: No or empty producer managed path was found");
        rc = DYAD_RC_BADMANAGEDPATH;
        goto produce_done;
    }
    // If the context is valid, call dyad_commit to perform
    // the producer operation
    rc = dyad_commit (ctx, fname);
    if (!DYAD_IS_ERROR (rc)) {
        // Best-effort: eviction failures must never turn a successful
        // produce into a reported error. No-ops if eviction is disabled.
        dyad_cache_maybe_evict (ctx, ctx->prod_managed_path);
    }
produce_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/** This function is coupled with Python API. This populates `mdata' which
 * is used by `dyad_consume_w_metadata ()'
 */
dyad_rc_t dyad_get_metadata (dyad_ctx_t *restrict ctx,
                             const char *restrict fname,
                             bool should_wait,
                             dyad_metadata_t **restrict mdata)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", fname);
    DYAD_C_FUNCTION_UPDATE_INT ("should_wait", should_wait);
    dyad_rc_t rc = DYAD_RC_OK;

#if 0
    if (fname == NULL || strlen (fname) > PATH_MAX) {
        rc = DYAD_RC_SYSFAIL;
        goto get_metadata_done;
    }
#endif
    const size_t fname_len = strlen (fname);
    char upath[PATH_MAX + 1] = {'\0'};

    if (fname_len == 0ul) {
        rc = DYAD_RC_BADFIO;
        DYAD_LOG_ERROR (ctx, "Filename length is zero");
        goto get_metadata_done;
    }
    if (ctx->relative_to_managed_path
        && (strncmp (fname, DYAD_PATH_DELIM, ctx->delim_len)
            != 0)) {  // fname is a relative path that is relative to the
                      // cons_managed_path
        memcpy (upath, fname, fname_len);
    } else if (!cmp_canonical_path_prefix (ctx, false, fname, upath, PATH_MAX)) {
        // Extract the path to the file specified by fname relative to the
        // producer-managed path
        // This relative path will be stored in upath
        // DYAD_LOG_TRACE (ctx, "%s is not in the Consumer's managed path\n",
        // fname);
        // NOTE: This is different from what dyad_fetch/commit returns,
        // which is DYAD_RC_OK such that dyad does not interfere accesses on
        // non-managed directories.
        rc = DYAD_RC_UNTRACKED;
        goto get_metadata_done;
    }
    DYAD_LOG_DEBUG (ctx,
                    "DYAD CLIENT: Obtaining file path relative to consumer directory: %s",
                    upath);
    ctx->reenter = false;
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);

    // check if file exist locally, if so skip kvs
    int fd = open (fname, O_RDONLY);
    if (fd != -1) {
        close (fd);
        if (mdata == NULL) {
            DYAD_LOG_ERROR (ctx,
                            "Metadata double pointer is NULL. "
                            "Cannot correctly create metadata object");
            rc = DYAD_RC_NOTFOUND;
            goto get_metadata_done;
        }
        if (*mdata != NULL) {
            DYAD_LOG_DEBUG (ctx,
                            "DYAD CLIENT: Metadata object is already allocated. Skipping "
                            "allocation");
        } else {
            *mdata = (dyad_metadata_t *)malloc (sizeof (struct dyad_metadata));
            if (*mdata == NULL) {
                DYAD_LOG_ERROR (ctx, "Cannot allocate memory for metadata object");
                rc = DYAD_RC_SYSFAIL;
                goto get_metadata_done;
            }
        }
        (*mdata)->fpath = (char *)malloc (fname_len + 1);
        if ((*mdata)->fpath == NULL) {
            DYAD_LOG_ERROR (ctx, "Cannot allocate memory for fpath in metadata object");
            rc = DYAD_RC_SYSFAIL;
            goto get_metadata_done;
        }
        memset ((*mdata)->fpath, '\0', fname_len + 1);
        memcpy ((*mdata)->fpath, fname, fname_len);
        (*mdata)->owner_rank = ctx->rank;
        rc = DYAD_RC_OK;
        goto get_metadata_done;
    }

    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1] = {'\0'};
    DYAD_LOG_INFO (ctx, "Generating KVS key: %s", topic);
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    rc = dyad_kvs_read (ctx, topic, upath, should_wait, mdata);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Could not read data from the KVS");
        goto get_metadata_done;
    }
    rc = DYAD_RC_OK;

get_metadata_done:;
    if (DYAD_IS_ERROR (rc) && mdata != NULL && *mdata != NULL) {
        dyad_free_metadata (mdata);
    }
    ctx->reenter = true;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_free_metadata (dyad_metadata_t **mdata)
{
    DYAD_C_FUNCTION_START ();
    if (mdata == NULL || *mdata == NULL) {
        return DYAD_RC_OK;
    }
    if ((*mdata)->fpath != NULL)
        free ((*mdata)->fpath);
    free (*mdata);
    *mdata = NULL;
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_consume (dyad_ctx_t *restrict ctx, const char *restrict fname)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", fname);
    dyad_rc_t rc = DYAD_RC_OK;
    int lock_fd = -1, io_fd = -1;
    ssize_t file_size = -1;
    char *file_data = NULL;
    size_t data_len = 0ul;
    dyad_metadata_t *mdata = NULL;
    struct flock exclusive_lock;
    char upath[PATH_MAX + 1] = {'\0'};

    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h) {
        rc = DYAD_RC_NOCTX;
        goto consume_close;
    }
    // If the consumer-managed path is NULL or empty, then the context is not
    // valid for a consumer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->cons_managed_path == NULL) {
        rc = DYAD_RC_BADMANAGEDPATH;
        goto consume_close;
    }

    if (ctx->relative_to_managed_path && (strlen (fname) > 0ul)
        && (strncmp (fname, DYAD_PATH_DELIM, ctx->delim_len)
            != 0)) {  // fname is a relative path that is relative to the
                      // cons_managed_path
        memcpy (upath, fname, strlen (fname));
    } else if (!cmp_canonical_path_prefix (ctx, false, fname, upath, PATH_MAX)) {
        // Extract the path to the file specified by fname relative to the
        // consumer-managed path
        // This relative path will be stored in upath
        // DYAD_LOG_TRACE (ctx, "%s is not in the Consumer's managed path\n",
        // fname);
        rc = DYAD_RC_OK;
        goto consume_close;
    }
    ctx->reenter = false;

    lock_fd = open (fname, O_RDWR | O_CREAT, 0666);
    if (lock_fd == -1) {
        // This could be a system file on which users have no write permission
        DYAD_LOG_ERROR (ctx, "Cannot create file (%s) for dyad_consume!\n", fname);
        rc = DYAD_RC_BADFIO;
        goto consume_close;
    }
    rc = dyad_excl_flock (ctx, lock_fd, &exclusive_lock);
    if (DYAD_IS_ERROR (rc)) {
        dyad_release_flock (ctx, lock_fd, &exclusive_lock);
        goto consume_done;
    }
    file_size = get_file_size (lock_fd);
    if (ctx->shared_storage) {
        dyad_release_flock (ctx, lock_fd, &exclusive_lock);
        if (!ctx->use_fs_locks || file_size <= 0) {
            // As file size being zero means that consumer won the lock first. So has to
            // wait for kvs. or we cannot use file lock based synchronization as it
            // does not work with the files managed by c++ fstream.
            rc = dyad_fetch_metadata (ctx, fname, upath, &mdata);
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "dyad_fetch_metadata failed for shared storage!\n");
                goto consume_done;
            }
        }
    } else {
        // When use_fs_locks is false, filesystem locking is unavailable
        // (e.g. DYAD_HAS_STD_FSTREAM_FD is not defined for C++ streams),
        // so we cannot rely on file size alone to determine if the file is
        // fully written as producer cannot lock the file it is still writting.
        // In that case, always fetch metadata from KVS to ensure correctness,
        // as in the shared storage path. For the C GOTCHA wrapper path,
        // use_fs_locks is irrelevant and should always be true.
        if (!ctx->use_fs_locks || file_size <= 0) {
            DYAD_LOG_INFO (ctx,
                           "[node %u rank %u pid %d] File (%s with lock_fd %d) is not "
                           "fetched yet",
                           ctx->node_idx,
                           ctx->rank,
                           ctx->pid,
                           fname,
                           lock_fd);
            // Call dyad_fetch to get (and possibly wait on)
            // data from the Flux KVS
            rc = dyad_fetch_metadata (ctx, fname, upath, &mdata);
            // If an error occured in dyad_fetch_metadata, log an error
            // and return the corresponding DYAD return code
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "dyad_fetch_metadata failed!\n");
                dyad_release_flock (ctx, lock_fd, &exclusive_lock);
                goto consume_done;
            }
            // If dyad_fetch_metadata was successful, but mdata is still NULL,
            // then we need to skip data transfer.
            // This is either because producer and consumer share storage
            // or because the file is not on the managed directory.
            if (mdata == NULL) {
                DYAD_LOG_INFO (ctx, "File '%s' is local!\n", fname);
                rc = DYAD_RC_OK;
                dyad_release_flock (ctx, lock_fd, &exclusive_lock);
                goto consume_done;
            }

            // Call dyad_get_data to dispatch a RPC to the producer's Flux broker
            // and retrieve the data associated with the file
            rc = dyad_get_data (ctx, mdata, &file_data, &data_len);
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "dyad_get_data failed!\n");
                dyad_release_flock (ctx, lock_fd, &exclusive_lock);
                goto consume_done;
            }
            DYAD_C_FUNCTION_UPDATE_INT ("data_len", data_len);
            io_fd = open (fname, O_WRONLY);
            DYAD_C_FUNCTION_UPDATE_INT ("io_fd", io_fd);
            if (io_fd == -1) {
                DYAD_LOG_ERROR (ctx,
                                "Cannot open file (%s) in write mode for dyad_consume!\n",
                                fname);
                rc = DYAD_RC_BADFIO;
                goto consume_close;
            }
            // Call dyad_pull to fetch the data from the producer's
            // Flux broker
            rc = dyad_cons_store (ctx, mdata, io_fd, data_len, file_data);
            // Regardless if there was an error in dyad_pull,
            // free the KVS response object
            if (mdata != NULL) {
                dyad_free_metadata (&mdata);
            }
            if (close (io_fd) != 0) {
                rc = DYAD_RC_BADFIO;
                dyad_release_flock (ctx, lock_fd, &exclusive_lock);
                goto consume_done;
            }
            // If an error occured in dyad_pull, log it
            // and return the corresponding DYAD return code
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "dyad_cons_store failed!\n");
                dyad_release_flock (ctx, lock_fd, &exclusive_lock);
                goto consume_done;
            };
        }
        dyad_release_flock (ctx, lock_fd, &exclusive_lock);
        // Best-effort: eviction failures must never turn a successful
        // consume into a reported error. No-ops if eviction is disabled.
        // Reached whether the file was already local or freshly fetched;
        // dyad_cache_maybe_evict()'s own capacity check makes the
        // already-under-budget case a cheap no-op either way.
        dyad_cache_maybe_evict (ctx, ctx->cons_managed_path);
    }
    DYAD_C_FUNCTION_UPDATE_INT ("file_size", file_size);
consume_done:;
    if (close (lock_fd) != 0) {
        rc = DYAD_RC_BADFIO;
    }
    if (file_data != NULL) {
        ctx->dtl_handle->return_buffer (ctx, (void **)&file_data);
    }
    // Set reenter to true to allow additional intercepting
consume_close:;
    ctx->reenter = true;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_consume_w_metadata (dyad_ctx_t *restrict ctx,
                                   const char *fname,
                                   const dyad_metadata_t *restrict mdata)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", fname);
    dyad_rc_t rc = DYAD_RC_OK;
    int lock_fd = -1, io_fd = -1;
    ssize_t file_size = -1;
    char *file_data = NULL;
    size_t data_len = 0ul;
    struct flock exclusive_lock;
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h) {
        rc = DYAD_RC_NOCTX;
        goto consume_close;
    }
    // If dyad_get_metadata was successful, but mdata is still NULL,
    // then we need to skip data transfer.
    if (mdata == NULL) {
        DYAD_LOG_INFO (ctx, "File '%s' is local!\n", fname);
        rc = DYAD_RC_OK;
        goto consume_close;
    }
    // If the consumer-managed path is NULL or empty, then the context is not
    // valid for a consumer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->cons_managed_path == NULL) {
        rc = DYAD_RC_BADMANAGEDPATH;
        goto consume_close;
    }
    // Set reenter to false to avoid recursively performing DYAD operations
    ctx->reenter = false;
    lock_fd = open (fname, O_RDWR | O_CREAT, 0666);
    DYAD_C_FUNCTION_UPDATE_INT ("lock_fd", lock_fd);
    if (lock_fd == -1) {
        DYAD_LOG_ERROR (ctx, "Cannot create file (%s) for dyad_consume_w_metadata!", fname);
        rc = DYAD_RC_BADFIO;
        goto consume_close;
    }
    rc = dyad_excl_flock (ctx, lock_fd, &exclusive_lock);
    if (DYAD_IS_ERROR (rc)) {
        dyad_release_flock (ctx, lock_fd, &exclusive_lock);
        goto consume_close;
    }
    if ((file_size = get_file_size (lock_fd)) <= 0) {
        DYAD_LOG_INFO (ctx,
                       "DYAD CLIENT: [node %u rank %u pid %d] File (%s with fd %d) is not fetched "
                       "yet",
                       ctx->node_idx,
                       ctx->rank,
                       ctx->pid,
                       fname,
                       lock_fd);

        // Call dyad_get_data to dispatch a RPC to the producer's Flux broker
        // and retrieve the data associated with the file
        rc = dyad_get_data (ctx, mdata, &file_data, &data_len);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (ctx, "dyad_get_data failed!\n");
            dyad_release_flock (ctx, lock_fd, &exclusive_lock);
            goto consume_done;
        }
        DYAD_C_FUNCTION_UPDATE_INT ("data_len", data_len);
        io_fd = open (fname, O_WRONLY);
        DYAD_C_FUNCTION_UPDATE_INT ("io_fd", io_fd);
        if (io_fd == -1) {
            DYAD_LOG_ERROR (ctx, "Cannot open file (%s) in write mode for dyad_consume!\n", fname);
            rc = DYAD_RC_BADFIO;
            goto consume_close;
        }
        // Call dyad_pull to fetch the data from the producer's
        // Flux broker
        rc = dyad_cons_store (ctx, mdata, io_fd, data_len, file_data);

        if (close (io_fd) != 0) {
            rc = DYAD_RC_BADFIO;
            dyad_release_flock (ctx, lock_fd, &exclusive_lock);
            goto consume_done;
        }
        // If an error occured in dyad_pull, log it
        // and return the corresponding DYAD return code
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (ctx, "dyad_cons_store failed!\n");
            dyad_release_flock (ctx, io_fd, &exclusive_lock);
            goto consume_done;
        };
    }
    dyad_release_flock (ctx, lock_fd, &exclusive_lock);
    // Best-effort: eviction failures must never turn a successful consume
    // into a reported error. No-ops if eviction is disabled. Reached
    // whether the file was already local or freshly fetched;
    // dyad_cache_maybe_evict()'s own capacity check makes the
    // already-under-budget case a cheap no-op either way.
    dyad_cache_maybe_evict (ctx, ctx->cons_managed_path);
    DYAD_C_FUNCTION_UPDATE_INT ("file_size", file_size);

    if (close (lock_fd) != 0) {
        rc = DYAD_RC_BADFIO;
        goto consume_done;
    }
    rc = DYAD_RC_OK;
consume_done:;
    if (file_data != NULL) {
        ctx->dtl_handle->return_buffer (ctx, (void **)&file_data);
    }
consume_close:;
    // Set reenter to true to allow additional intercepting
    ctx->reenter = true;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_consume_range (dyad_ctx_t *restrict ctx,
                              const char *restrict fname,
                              size_t offset,
                              size_t length,
                              void **restrict data,
                              size_t *restrict data_len)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("fname", fname);
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_metadata_t *mdata = NULL;
    char upath[PATH_MAX + 1] = {'\0'};
    char fullpath[PATH_MAX + 1] = {'\0'};
    int fd = -1;
    ssize_t nread = 0;
    struct flock shared_lock;

    *data = NULL;
    *data_len = 0ul;

    if (!ctx || !ctx->h) {
        rc = DYAD_RC_NOCTX;
        goto consume_range_close;
    }
    if (ctx->cons_managed_path == NULL) {
        rc = DYAD_RC_BADMANAGEDPATH;
        goto consume_range_close;
    }
    if (ctx->dtl_handle == NULL || ctx->dtl_handle->mode == DYAD_DTL_UCX) {
        DYAD_LOG_ERROR (ctx, "dyad_consume_range is not supported for the active DTL mode (UCX)");
        rc = DYAD_RC_BADDTLMODE;
        goto consume_range_close;
    }

    if (ctx->relative_to_managed_path && (strlen (fname) > 0ul)
        && (strncmp (fname, DYAD_PATH_DELIM, ctx->delim_len) != 0)) {
        memcpy (upath, fname, strlen (fname));
    } else if (!cmp_canonical_path_prefix (ctx, false, fname, upath, PATH_MAX)) {
        DYAD_LOG_ERROR (ctx,
                        "dyad_consume_range: '%s' is not under the consumer-managed path",
                        fname);
        rc = DYAD_RC_BADMANAGEDPATH;
        goto consume_range_close;
    }
    ctx->reenter = false;

    rc = dyad_fetch_metadata (ctx, fname, upath, &mdata);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "dyad_consume_range: dyad_fetch_metadata failed!");
        goto consume_range_done;
    }

    if (mdata == NULL) {
        // File is already local to this node (per dyad_fetch_metadata's
        // existing HFL/DMD check) -- pread the local copy directly, no RPC.
        strncpy (fullpath, ctx->cons_managed_path, PATH_MAX - 1);
        concat_str (fullpath, upath, "/", PATH_MAX);
        if (ctx->origin_path != NULL) {
            char origin_fullpath[PATH_MAX + 1] = {'\0'};
            strncpy (origin_fullpath, ctx->origin_path, PATH_MAX - 1);
            concat_str (origin_fullpath, upath, "/", PATH_MAX);
            rc = dyad_range_cache_ensure (ctx, fullpath, origin_fullpath, offset, length);
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx,
                                "dyad_consume_range: dyad_range_cache_ensure failed for local "
                                "file '%s'",
                                fullpath);
                goto consume_range_done;
            }
        }
        fd = open (fullpath, O_RDONLY);
        if (fd == -1) {
            DYAD_LOG_ERROR (ctx, "dyad_consume_range: cannot open local file '%s'", fullpath);
            rc = DYAD_RC_BADFIO;
            goto consume_range_done;
        }
        rc = dyad_shared_flock (ctx, fd, &shared_lock);
        if (DYAD_IS_ERROR (rc)) {
            close (fd);
            goto consume_range_done;
        }
        *data = malloc (length);
        if (*data == NULL) {
            rc = DYAD_RC_SYSFAIL;
            dyad_release_flock (ctx, fd, &shared_lock);
            close (fd);
            goto consume_range_done;
        }
        nread = pread (fd, *data, length, (off_t)offset);
        dyad_release_flock (ctx, fd, &shared_lock);
        close (fd);
        if (nread < 0 || (size_t)nread != length) {
            DYAD_LOG_ERROR (ctx,
                            "dyad_consume_range: pread of local file '%s' failed (got %zd of "
                            "%zu)",
                            fullpath,
                            nread,
                            length);
            free (*data);
            *data = NULL;
            rc = DYAD_RC_BADFIO;
            goto consume_range_done;
        }
        *data_len = length;
    } else {
        // File owned by a remote broker -- fetch just the requested range.
        rc = dyad_get_data_range (ctx, mdata, offset, length, (char **)data, data_len);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (ctx, "dyad_consume_range: dyad_get_data_range failed!");
            goto consume_range_done;
        }
    }
    rc = DYAD_RC_OK;

consume_range_done:;
    if (mdata != NULL) {
        dyad_free_metadata (&mdata);
    }
consume_range_close:;
    ctx->reenter = true;
    DYAD_C_FUNCTION_END ();
    return rc;
}

#if DYAD_SYNC_DIR
/**
 * @brief Synchronizes the parent directory of a file to ensure its entry is
 *        durably written to storage.
 *
 * @details
 * Opens the parent directory of @p path and calls @c fsync() on it to flush
 * any pending directory entry updates to stable storage. This is necessary
 * after creating a new file to guarantee that the directory entry is durable
 * and visible to other processes, even in the event of a system crash. See
 * https://lwn.net/Articles/457671/ for background.
 *
 * This is particularly important in the DYAD producer path — calling
 * @c dyad_commit() to publish a file to the KVS before the directory entry
 * is synced could allow a consumer to attempt to open the file before it is
 * visible in the directory.
 *
 * @c ctx->reenter is saved and restored around the directory open and sync
 * operations to prevent re-entrant interception of the @c open() call.
 *
 * @param[in]     ctx   Pointer to the DYAD context. May be @c NULL, in which
 *                      case the @c reenter guard is skipped. If non-@c NULL,
 *                      @c ctx->reenter is temporarily set to @c false during
 *                      the operation.
 * @param[in]     path  Path to the file whose parent directory is to be synced.
 *                      Must not be @c NULL. The parent directory is derived via
 *                      @c dirname().
 *
 * @return int
 * @retval  0  The parent directory was successfully opened, synced, and closed.
 * @retval -1  The parent directory could not be opened, @c fsync() failed, or
 *             @c close() failed.
 *
 * @note All three operations (open, fsync, close) are attempted even if one
 *       fails — the return code reflects whether any of them failed, but does
 *       not distinguish which one.
 */
int dyad_sync_directory (dyad_ctx_t *restrict ctx, const char *restrict path)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", path);
    // Flush new directory entry https://lwn.net/Articles/457671/
    char path_copy[PATH_MAX + 1] = {'\0'};
    int odir_fd = -1;
    char *odir = NULL;
    bool reenter = false;
    int rc = 0;
    memset (path_copy, 0, PATH_MAX + 1);

    strncpy (path_copy, path, PATH_MAX);
    odir = dirname (path_copy);

    reenter = ctx->reenter;  // backup ctx->reenter
    if (ctx != NULL)
        ctx->reenter = false;

    if ((odir_fd = open (odir, O_RDONLY)) < 0) {
        IPRINTF (ctx, "Cannot open the directory \"%s\"\n", odir);
        rc = -1;
    } else {
        if (fsync (odir_fd) < 0) {
            IPRINTF (ctx, "Cannot flush the directory \"%s\"\n", odir);
            rc = -1;
        }
        if (close (odir_fd) < 0) {
            IPRINTF (ctx, "Cannot close the directory \"%s\"\n", odir);
            rc = -1;
        }
    }
    if (ctx != NULL)
        ctx->reenter = reenter;
    DYAD_C_FUNCTION_END ();
    return rc;
}
#endif
