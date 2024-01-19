#include <dyad/core/dyad_core.h>

#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>

#include <dyad/core/dyad_envs.h>
#include <dyad/dtl/dyad_dtl_impl.h>
#include <dyad/common/dyad_flux_log.h>
#include <dyad/perf/dyad_perf.h>
#include <dyad/utils/murmur3.h>
#include <dyad/utils/utils.h>

#ifdef __cplusplus
#include <climits>
#include <cstring>
#else
#include <limits.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#endif

#if DYAD_PERFFLOW
#define DYAD_CORE_FUNC_MODS __attribute__ ((annotate ("@critical_path()"))) static
#else
#define DYAD_CORE_FUNC_MODS static inline
#endif

const struct dyad_ctx dyad_ctx_default = {
    NULL,   // h
    NULL,   // dtl_handle
    NULL,   // perf_handle
    false,  // debug
    false,  // check
    false,  // reenter
    true,   // initialized
    false,  // shared_storage
    3u,     // key_depth
    1024u,  // key_bins
    0u,     // rank
    1u,     // service_mux
    0u,     // node_idx
    -1,     // pid
    NULL,   // kvs_namespace
    NULL,   // prod_managed_path
    NULL    // cons_managed_path
};

static int gen_path_key (const char* str,
                         char* path_key,
                         const size_t len,
                         const uint32_t depth,
                         const uint32_t width)
{
    static const uint32_t seeds[10] = {104677u,
                                       104681u,
                                       104683u,
                                       104693u,
                                       104701u,
                                       104707u,
                                       104711u,
                                       104717u,
                                       104723u,
                                       104729u};

    uint32_t seed = 57u;
    uint32_t hash[4] = {0u};  // Output for the hash
    size_t cx = 0ul;
    int n = 0;

    if (path_key == NULL || len == 0ul) {
        return -1;
    }
    path_key[0] = '\0';

    for (uint32_t d = 0u; d < depth; d++) {
        seed += seeds[d % 10];
        // TODO add assert that str is not NULL
        MurmurHash3_x64_128 (str, strlen (str), seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key + cx, len - cx, "%x.", bin);
        cx += n;
        if (cx >= len || n < 0) {
            return -1;
        }
    }
    n = snprintf (path_key + cx, len - cx, "%s", str);
    if (cx + n >= len || n < 0) {
        return -1;
    }
    return 0;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_kvs_commit (const dyad_ctx_t* ctx, flux_kvs_txn_t* txn)
{
    flux_future_t* f = NULL;
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_kvs_commit");
    DYAD_LOG_INFO (ctx, "Committing transaction to KVS\n");
    // Commit the transaction to the Flux KVS
    f = flux_kvs_commit (ctx->h, ctx->kvs_namespace, 0, txn);
    // If the commit failed, log an error and return DYAD_BADCOMMIT
    if (f == NULL) {
        DYAD_LOG_ERR (ctx, "Could not commit transaction to Flux KVS\n");
        rc = DYAD_RC_BADCOMMIT;
        goto kvs_commit_region_finish;
    }
    // If the commit is pending, wait for it to complete
    flux_future_wait_for (f, -1.0);
    // Once the commit is complete, destroy the future and transaction
    flux_future_destroy (f);
    f = NULL;
    rc = DYAD_RC_OK;
kvs_commit_region_finish:
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_kvs_commit");
    return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t publish_via_flux (const dyad_ctx_t* restrict ctx,
                                                const char* restrict upath)
{
    dyad_rc_t rc = DYAD_RC_OK;
    flux_kvs_txn_t* txn = NULL;
    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1];
    memset (topic, 0, topic_len + 1);
    memset (topic, '\0', topic_len + 1);
    // Generate the KVS key from the file path relative to
    // the producer-managed directory
    DYAD_LOG_INFO (ctx, "Generating KVS key from path (%s)\n", upath);
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    // Crete and pack a Flux KVS transaction.
    // The transaction will contain a single key-value pair
    // with the previously generated key as the key and the
    // producer's rank as the value
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_kvs_transaction_create_and_pack");
    DYAD_LOG_INFO (ctx, "Creating KVS transaction under the key %s\n", topic);
    txn = flux_kvs_txn_create ();
    if (txn == NULL) {
        DYAD_LOG_ERR (ctx, "Could not create Flux KVS transaction\n");
        rc = DYAD_RC_FLUXFAIL;
        goto kvs_transaction_create_and_pack_region_finish;
    }
    if (flux_kvs_txn_pack (txn, 0, topic, "i", ctx->rank) < 0) {
        DYAD_LOG_ERR (ctx, "Could not pack Flux KVS transaction\n");
        rc = DYAD_RC_FLUXFAIL;
        goto kvs_transaction_create_and_pack_region_finish;
    }
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_kvs_transaction_create_and_pack");
    // Call dyad_kvs_commit to commit the transaction into the Flux KVS
    rc = dyad_kvs_commit (ctx, txn);
    // If dyad_kvs_commit failed, log an error and forward the return code
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx, "dyad_kvs_commit failed!\n");
        goto publish_done;
    }
    rc = DYAD_RC_OK;
publish_done:;
    if (txn != NULL) {
        flux_kvs_txn_destroy (txn);
    }
    return rc;
kvs_transaction_create_and_pack_region_finish:
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_kvs_transaction_create_and_pack");
    goto publish_done;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_commit (dyad_ctx_t* restrict ctx,
                                           const char* restrict fname)
{
    dyad_rc_t rc = DYAD_RC_OK;
    char upath[PATH_MAX];
    memset (upath, 0, PATH_MAX);
    // Extract the path to the file specified by fname relative to the
    // producer-managed path
    // This relative path will be stored in upath
    if (!cmp_canonical_path_prefix (ctx->prod_managed_path, fname, upath, PATH_MAX)) {
        DYAD_LOG_INFO (ctx, "%s is not in the Producer's managed path\n", fname);
        rc = DYAD_RC_OK;
        goto commit_done;
    }
    DYAD_LOG_INFO (ctx, "Obtained file path relative to producer directory: %s\n", upath);
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
    return rc;
}

static void print_mdata (const dyad_ctx_t* restrict ctx,
                         const dyad_metadata_t* mdata)
{
    if (mdata == NULL) {
        DYAD_LOG_INFO (ctx, "Cannot print a NULL metadata object!");
    } else {
        DYAD_LOG_INFO (ctx, "Printing contents of DYAD Metadata object");
        DYAD_LOG_INFO (ctx, "fpath = %s", mdata->fpath);
        DYAD_LOG_INFO (ctx, "owner_rank = %u", mdata->owner_rank);
    }
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_kvs_read (const dyad_ctx_t* restrict ctx,
                                             const char* restrict topic,
                                             const char* restrict upath,
                                             bool should_wait,
                                             dyad_metadata_t** mdata)
{
    dyad_rc_t rc = DYAD_RC_OK;
    int kvs_lookup_flags = 0;
    flux_future_t* f = NULL;
    if (mdata == NULL) {
        DYAD_LOG_ERR (ctx,
                      "Metadata double pointer is NULL. Cannot correctly create metadata "
                      "object");
        rc = DYAD_RC_NOTFOUND;
        goto kvs_read_end;
    }
    // Lookup information about the desired file (represented by kvs_topic)
    // from the Flux KVS. If there is no information, wait for it to be
    // made available
    if (should_wait)
        kvs_lookup_flags = FLUX_KVS_WAITCREATE;
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_kvs_lookup");
    DYAD_LOG_INFO (ctx, "Retrieving information from KVS under the key %s\n", topic);
    f = flux_kvs_lookup (ctx->h, ctx->kvs_namespace, kvs_lookup_flags, topic);
    // If the KVS lookup failed, log an error and return DYAD_BADLOOKUP
    if (f == NULL) {
        DYAD_LOG_ERR (ctx, "KVS lookup failed!\n");
        DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_kvs_lookup");
        rc = DYAD_RC_NOTFOUND;
        goto kvs_read_end;
    }
    // Extract the rank of the producer from the KVS response
    DYAD_LOG_INFO (ctx, "Building metadata object from KVS entry\n");
    if (*mdata != NULL) {
        DYAD_LOG_INFO (ctx, "Metadata object is already allocated. Skipping allocation");
    } else {
        *mdata = (dyad_metadata_t*)malloc (sizeof (struct dyad_metadata));
        if (*mdata == NULL) {
            DYAD_LOG_ERR (ctx, "Cannot allocate memory for metadata object");
            rc = DYAD_RC_SYSFAIL;
            goto kvs_read_end;
        }
    }
    size_t upath_len = strlen (upath);
    (*mdata)->fpath = (char*)malloc (upath_len + 1);
    if ((*mdata)->fpath == NULL) {
        DYAD_LOG_ERR (ctx, "Cannot allocate memory for fpath in metadata object");
        rc = DYAD_RC_SYSFAIL;
        goto kvs_read_end;
    }
    memset ((*mdata)->fpath, '\0', upath_len + 1);
    strncpy ((*mdata)->fpath, upath, upath_len);
    rc = flux_kvs_lookup_get_unpack (f, "i", &((*mdata)->owner_rank));
    // If the extraction did not work, log an error and return DYAD_BADFETCH
    if (rc < 0) {
        DYAD_LOG_ERR (ctx, "Could not unpack owner's rank from KVS response\n");
        rc = DYAD_RC_BADMETADATA;
        goto kvs_read_end;
    }
    DYAD_LOG_INFO (ctx, "Successfully created DYAD Metadata object");
    print_mdata (ctx, *mdata);
    rc = DYAD_RC_OK;

kvs_read_end:
    if (DYAD_IS_ERROR (rc) && mdata != NULL && *mdata != NULL) {
        dyad_free_metadata (mdata);
    }
    if (f != NULL) {
        flux_future_destroy (f);
        f = NULL;
    }
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_kvs_lookup");
    return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_excl_flock (const dyad_ctx_t* restrict ctx,
                                               int fd, struct flock* restrict lock)
{
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Applies an exclusive lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) return DYAD_RC_BADFIO;
    lock->l_type = F_WRLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid; //getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) { // will wait until able to lock
        DYAD_LOG_ERR (ctx, "Cannot apply exclusive lock on fd %d", fd);
        return DYAD_RC_BADFIO;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Exclusive lock placed on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    return DYAD_RC_OK;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_shared_flock (const dyad_ctx_t* restrict ctx,
                                                 int fd, struct flock* restrict lock)
{
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Applies a shared lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) return DYAD_RC_BADFIO;
    lock->l_type = F_RDLCK;
    lock->l_whence = SEEK_SET;
    lock->l_start = 0;
    lock->l_len = 0;
    lock->l_pid = ctx->pid; //getpid();
    if (fcntl (fd, F_SETLKW, lock) == -1) { // will wait until able to lock
        DYAD_LOG_ERR (ctx, "Cannot apply shared lock on fd %d", fd);
        return DYAD_RC_BADFIO;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Shared lock placed on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    return DYAD_RC_OK;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_release_flock (const dyad_ctx_t* restrict ctx,
                                                  int fd, struct flock* restrict lock)
{
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] Releases a lock on fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    if (!lock) return DYAD_RC_BADFIO;
    lock->l_type = F_UNLCK;
    if (fcntl (fd, F_SETLK, lock) == -1) { // will just unlock
        DYAD_LOG_ERR (ctx, "Cannot release lock on fd %d", fd);
        return DYAD_RC_BADFIO;
    }
    DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] lock lifted from fd %d", \
                   ctx->node_idx, ctx->rank, ctx->pid, fd);
    return DYAD_RC_OK;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_fetch (const dyad_ctx_t* restrict ctx,
                                          const char* restrict fname,
                                          dyad_metadata_t** restrict mdata)
{
    dyad_rc_t rc = DYAD_RC_OK;
    char upath[PATH_MAX];
    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1];
    int fd = -1;
    memset (upath, 0, PATH_MAX);
    memset (topic, 0, topic_len + 1);
    // Extract the path to the file specified by fname relative to the
    // consumer-managed path
    // This relative path will be stored in upath
    if (!cmp_canonical_path_prefix (ctx->cons_managed_path, fname, upath, PATH_MAX)) {
        DYAD_LOG_INFO (ctx, "%s is not in the Consumer's managed path\n", fname);
        rc = DYAD_RC_OK;
        goto fetch_done;
    }
    DYAD_LOG_INFO (ctx, "Obtained file path relative to consumer directory: %s\n", upath);
    // Generate the KVS key from the file path relative to
    // the consumer-managed directory
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    DYAD_LOG_INFO (ctx, "Generated KVS key for consumer: %s\n", topic);
    // Call dyad_kvs_read to retrieve infromation about the file
    // from the Flux KVS
    rc = dyad_kvs_read (ctx, topic, upath, true, mdata);
    // If an error occured in dyad_kvs_read, log it and propagate the return
    // code
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx, "dyad_kvs_read failed!\n");
        goto fetch_done;
    }
    // There are two cases where we do not want to perform file transfer:
    //   1. if the shared storage feature is enabled
    //   2. if the producer and consumer share the same rank
    // In either of these cases, skip the creation of the dyad_kvs_response_t
    // object, and return DYAD_OK. This will cause the file transfer step to be
    // skipped
    if (ctx->shared_storage ||
        (((*mdata)->owner_rank / ctx->service_mux) == ctx->node_idx)) {
        DYAD_LOG_INFO (ctx,
                       "Either shared-storage is enabled or the producer rank (%u) "
                       "is the "
                       "same as the consumer rank (%u)", (*mdata)->owner_rank, ctx->rank);
        if (mdata != NULL && *mdata != NULL) {
            dyad_free_metadata (mdata);
        }
        rc = DYAD_RC_OK;
        goto fetch_done;
    }
    rc = DYAD_RC_OK;

fetch_done:;
    return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_get_data (const dyad_ctx_t* ctx,
                                             const dyad_metadata_t* restrict mdata,
                                             char** file_data,
                                             size_t* file_len)
{
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_rc_t final_rc = DYAD_RC_OK;
    flux_future_t* f;
    json_t* rpc_payload;
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_get_data");
    DYAD_LOG_INFO (ctx, "Packing payload for RPC to DYAD module");
    rc = ctx->dtl_handle->rpc_pack (ctx->dtl_handle,
                                    mdata->fpath,
                                    mdata->owner_rank,
                                    &rpc_payload);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx,
                      "Cannot create JSON payload for Flux RPC to DYAD "
                      "module\n");
        goto get_done;
    }
    DYAD_LOG_INFO (ctx, "Sending payload for RPC to DYAD module");
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_send_rpc");
    f = flux_rpc_pack (ctx->h,
                       DYAD_DTL_RPC_NAME,
                       mdata->owner_rank,
                       FLUX_RPC_STREAMING,
                       "o",
                       rpc_payload);
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_send_rpc");
    if (f == NULL) {
        DYAD_LOG_ERR (ctx, "Cannot send RPC to producer module\n");
        rc = DYAD_RC_BADRPC;
        goto get_done;
    }
    DYAD_LOG_INFO (ctx, "Receive RPC response from DYAD module");
    rc = ctx->dtl_handle->rpc_recv_response (ctx->dtl_handle, f);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx, "Cannot receive and/or parse the RPC response\n");
        goto get_done;
    }
    DYAD_LOG_INFO (ctx, "Establish DTL connection with DYAD module");
    rc = ctx->dtl_handle->establish_connection (ctx->dtl_handle, DYAD_COMM_RECV);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx,
                      "Cannot establish connection with DYAD module on broker "
                      "%u\n",
                      mdata->owner_rank);
        goto get_done;
    }
    DYAD_LOG_INFO (ctx, "Receive file data via DTL");
    rc = ctx->dtl_handle->recv (ctx->dtl_handle, (void**)file_data, file_len);
    DYAD_LOG_INFO (ctx, "Close DTL connection with DYAD module");
    ctx->dtl_handle->close_connection (ctx->dtl_handle);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx, "Cannot receive data from producer module\n");
        goto get_done;
    }

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
    DYAD_LOG_INFO (ctx, "Wait for end-of-stream message from module (current RC = %d)\n", rc);
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_wait_for_rpc_end");
    if (rc != DYAD_RC_RPC_FINISHED && rc != DYAD_RC_BADRPC) {
        if (!(flux_rpc_get (f, NULL) < 0 && errno == ENODATA)) {
            DYAD_LOG_ERR (ctx,
                          "An error occured at end of getting data! Either the "
                          "module sent too many responses, or the module "
                          "failed with a bad error (errno = %d)\n",
                          errno);
            rc = DYAD_RC_BADRPC;
        }
    }
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_wait_for_rpc_end");
    DYAD_LOG_INFO (ctx, "Destroy the Flux future for the RPC\n");
    flux_future_destroy (f);
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_get_data");
    return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_cons_store (const dyad_ctx_t* restrict ctx,
                                               const dyad_metadata_t* restrict mdata,
                                               int fd, const size_t data_len,
                                               const char* restrict file_data)
{
    dyad_rc_t rc = DYAD_RC_OK;
    const char* odir = NULL;
    FILE* of = NULL;
    char file_path[PATH_MAX + 1];
    char file_path_copy[PATH_MAX + 1];
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    size_t written_len = 0;
    memset (file_path, 0, PATH_MAX + 1);
    memset (file_path_copy, 0, PATH_MAX + 1);

    // Build the full path to the file being consumed
    DYAD_PERF_REGION_BEGIN (ctx->perf_handle, "dyad_write_data_for_consumer");
    strncpy (file_path, ctx->cons_managed_path, PATH_MAX - 1);
    concat_str (file_path, mdata->fpath, "/", PATH_MAX);
    strncpy (file_path_copy, file_path, PATH_MAX);  // dirname modifies the arg

    DYAD_LOG_INFO (ctx, "Saving retrieved data to %s\n", file_path);
    // Create the directory as needed
    // TODO: Need to be consistent with the mode at the source
    odir = dirname (file_path_copy);
    if ((strncmp (odir, ".", strlen (".")) != 0) && (mkdir_as_needed (odir, m) < 0)) {
        DYAD_LOG_ERR (ctx,
                      "Cannot create needed directories for pulled "
                      "file\n");
        rc = DYAD_RC_BADFIO;
        goto pull_done;
    }

    // Write the file contents to the location specified by the user
    written_len = write (fd, file_data, data_len);
    if (written_len != data_len) {
        DYAD_LOG_ERR (ctx, "cons store write of pulled file failed!\n");
        rc = DYAD_RC_BADFIO;
        goto pull_done;
    }
    rc = DYAD_RC_OK;

pull_done:
    DYAD_PERF_REGION_END (ctx->perf_handle, "dyad_write_data_for_consumer");
    if (file_data != NULL) {
        free ((void*)file_data);
    }
    // If "check" is set and the operation was successful, set the
    // DYAD_CHECK_ENV environment variable to "ok"
    if (rc == DYAD_RC_OK && (ctx && ctx->check))
        setenv(DYAD_CHECK_ENV, "ok", 1);
    return rc;
}

dyad_rc_t dyad_init (bool debug,
                     bool check,
                     bool shared_storage,
                     unsigned int key_depth,
                     unsigned int key_bins,
                     unsigned int service_mux,
                     const char* kvs_namespace,
                     const char* prod_managed_path,
                     const char* cons_managed_path,
                     dyad_dtl_mode_t dtl_mode,
                     dyad_ctx_t** ctx)
{
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_perf_t* perf_handle = NULL;
    rc = dyad_perf_init (&perf_handle, false, NULL);
    if (DYAD_IS_ERROR (rc)) {
        goto init_region_finish;
    }
    DYAD_PERF_REGION_BEGIN (perf_handle, "dyad_init");
    // If ctx is NULL, we won't be able to return a dyad_ctx_t
    // to the user. In that case, print an error and return
    // immediately with DYAD_NOCTX.
    if (ctx == NULL) {
        fprintf (stderr,
                 "'ctx' argument to dyad_init is NULL! This prevents us from "
                 "returning "
                 "a dyad_ctx_t object!\n");
        rc = DYAD_RC_NOCTX;
        goto init_region_finish;
    }
    // Check if the actual dyad_ctx_t object is not NULL.
    // If it is not NULL, that means the dyad_ctx_t object
    // has either been allocated or fully initialized.
    // If it's initialized, simply print a message and
    // return DYAD_OK.
    if (*ctx != NULL) {
        if ((*ctx)->initialized) {
            // TODO Indicate already initialized
            DPRINTF ((*ctx), "DYAD context already initialized\n");
            rc = DYAD_RC_OK;
            goto init_region_finish;
        }
    } else {
        // Allocate the dyad_ctx_t object and make sure the allocation
        // worked successfully
        *ctx = (dyad_ctx_t*)malloc (sizeof (struct dyad_ctx));
        if (*ctx == NULL) {
            fprintf (stderr, "Could not allocate DYAD context!\n");
            rc = DYAD_RC_NOCTX;
            goto init_region_finish;
        }
    }
    // Set the initial contents of the dyad_ctx_t object
    // to dyad_ctx_default.
    **ctx = dyad_ctx_default;
    // If neither managed path is provided, DYAD will not do anything.
    // So, simply print a warning and return DYAD_OK.
    if (prod_managed_path == NULL && cons_managed_path == NULL) {
        fprintf (stderr,
                 "Warning: no managed path provided! DYAD will not do "
                 "anything!\n");
        rc = DYAD_RC_OK;
        goto init_region_finish;
    }
    // Set the values in dyad_ctx_t that don't need allocation
    (*ctx)->perf_handle = perf_handle;
    (*ctx)->debug = debug;
    (*ctx)->check = check;
    (*ctx)->shared_storage = shared_storage;
    (*ctx)->key_depth = key_depth;
    (*ctx)->key_bins = key_bins;
    // Open a Flux handle and store it in the dyad_ctx_t
    // object. If the open operation failed, return DYAD_FLUXFAIL
    (*ctx)->h = flux_open (NULL, 0);
    if ((*ctx)->h == NULL) {
        fprintf (stderr, "Could not open Flux handle!\n");
        rc = DYAD_RC_FLUXFAIL;
        goto init_region_finish;
    }
    // Get the rank of the Flux broker corresponding
    // to the handle. If this fails, return DYAD_FLUXFAIL
    FLUX_LOG_INFO ((*ctx)->h, "DYAD_CORE: getting Flux rank");
    if (flux_get_rank ((*ctx)->h, &((*ctx)->rank)) < 0u) {
        FLUX_LOG_ERR ((*ctx)->h, "Could not get Flux rank!\n");
        rc = DYAD_RC_FLUXFAIL;
        goto init_region_finish;
    }
    (*ctx)->service_mux = (service_mux < 1u)? 1u : service_mux;
    (*ctx)->node_idx = (*ctx)->rank / (*ctx)->service_mux;
    (*ctx)->pid = getpid ();
    // If the namespace is provided, copy it into the dyad_ctx_t object
    FLUX_LOG_INFO ((*ctx)->h, "DYAD_CORE: saving KVS namespace");
    if (kvs_namespace == NULL) {
        FLUX_LOG_ERR ((*ctx)->h, "No KVS namespace provided!\n");
        // TODO see if we want a different return val
        rc = DYAD_RC_NOCTX;
        goto init_region_finish;
    }
    const size_t namespace_len = strlen (kvs_namespace);
    (*ctx)->kvs_namespace = (char*)malloc (namespace_len + 1);
    if ((*ctx)->kvs_namespace == NULL) {
        FLUX_LOG_ERR ((*ctx)->h, "Could not allocate buffer for KVS namespace!\n");
        free (*ctx);
        *ctx = NULL;
        rc = DYAD_RC_NOCTX;
        goto init_region_finish;
    }
    strncpy ((*ctx)->kvs_namespace, kvs_namespace, namespace_len + 1);
    // Initialize the DTL based on the value of dtl_mode
    // If an error occurs, log it and return an error
    FLUX_LOG_INFO ((*ctx)->h, "DYAD_CORE: inintializing DYAD DTL");
    rc = dyad_dtl_init (&(*ctx)->dtl_handle, dtl_mode, (*ctx)->h, (*ctx)->debug, (*ctx)->perf_handle);
    if (DYAD_IS_ERROR (rc)) {
        FLUX_LOG_ERR ((*ctx)->h, "Cannot initialize the DTL\n");
        goto init_region_finish;
    }
    // If the producer-managed path is provided, copy it into
    // the dyad_ctx_t object
    FLUX_LOG_INFO ((*ctx)->h, "DYAD_CORE: saving producer path");
    if (prod_managed_path == NULL) {
        (*ctx)->prod_managed_path = NULL;
    } else {
        const size_t prod_path_len = strlen (prod_managed_path);
        (*ctx)->prod_managed_path = (char*)malloc (prod_path_len + 1);
        if ((*ctx)->prod_managed_path == NULL) {
            FLUX_LOG_ERR ((*ctx)->h,
                          "Could not allocate buffer for Producer managed "
                          "path!\n");
            free ((*ctx)->kvs_namespace);
            free (*ctx);
            *ctx = NULL;
            rc = DYAD_RC_NOCTX;
            goto init_region_finish;
        }
        strncpy ((*ctx)->prod_managed_path, prod_managed_path, prod_path_len + 1);
    }
    // If the consumer-managed path is provided, copy it into
    // the dyad_ctx_t object
    FLUX_LOG_INFO ((*ctx)->h, "DYAD_CORE: saving consumer path");
    if (cons_managed_path == NULL) {
        (*ctx)->cons_managed_path = NULL;
    } else {
        const size_t cons_path_len = strlen (cons_managed_path);
        (*ctx)->cons_managed_path = (char*)malloc (cons_path_len + 1);
        if ((*ctx)->cons_managed_path == NULL) {
            FLUX_LOG_ERR ((*ctx)->h,
                          "Could not allocate buffer for Consumer managed "
                          "path!\n");
            free ((*ctx)->kvs_namespace);
            free ((*ctx)->prod_managed_path);
            free (*ctx);
            *ctx = NULL;
            rc = DYAD_RC_NOCTX;
            goto init_region_finish;
        }
        strncpy ((*ctx)->cons_managed_path, cons_managed_path, cons_path_len + 1);
    }
    // Initialization is now complete!
    // Set reenter and initialized to indicate this.
    (*ctx)->reenter = true;
    (*ctx)->initialized = true;
    // TODO Print logging info
    rc = DYAD_RC_OK;

init_region_finish:
    DYAD_PERF_REGION_END ((*ctx)->perf_handle, "dyad_init");
    return rc;
}

dyad_rc_t dyad_init_env (dyad_ctx_t** ctx)
{
    char* e = NULL;
    bool debug = false;
    bool check = false;
    bool shared_storage = false;
    unsigned int key_depth = 0u;
    unsigned int key_bins = 0u;
    unsigned int service_mux = 1u;
    char* kvs_namespace = NULL;
    char* prod_managed_path = NULL;
    char* cons_managed_path = NULL;
    size_t dtl_mode_env_len = 0ul;
    dyad_dtl_mode_t dtl_mode = DYAD_DTL_UCX;

    if ((e = getenv (DYAD_SYNC_DEBUG_ENV))) {
        debug = true;
        enable_debug_dyad_utils ();
    } else {
        debug = false;
        disable_debug_dyad_utils ();
    }

    if (debug)
        fprintf (stderr,
                 "DYAD_CORE: Initializing with environment "
                 "variables\n");

    if ((e = getenv (DYAD_SYNC_CHECK_ENV))) {
        check = true;
    } else {
        check = false;
    }

    if ((e = getenv (DYAD_SHARED_STORAGE_ENV))) {
        shared_storage = true;
    } else {
        shared_storage = false;
    }

    if ((e = getenv (DYAD_KEY_DEPTH_ENV))) {
        key_depth = atoi (e);
    } else {
        key_depth = 3;
    }

    if ((e = getenv (DYAD_KEY_BINS_ENV))) {
        key_bins = atoi (e);
    } else {
        key_bins = 1024;
    }

    if ((e = getenv (DYAD_SERVICE_MUX_ENV))) {
        service_mux = atoi (e);
    } else {
        service_mux= 1u;
    }

    if ((e = getenv (DYAD_KVS_NAMESPACE_ENV))) {
        kvs_namespace = e;
    } else {
        kvs_namespace = NULL;
    }

    if ((e = getenv (DYAD_PATH_CONSUMER_ENV))) {
        cons_managed_path = e;
    } else {
        cons_managed_path = NULL;
    }

    if ((e = getenv (DYAD_PATH_PRODUCER_ENV))) {
        prod_managed_path = e;
    } else {
        prod_managed_path = NULL;
    }

    if ((e = getenv (DYAD_DTL_MODE_ENV))) {
        dtl_mode_env_len = strlen (e);
        if (strncmp (e, "FLUX_RPC", dtl_mode_env_len) == 0) {
            dtl_mode = DYAD_DTL_FLUX_RPC;
        } else if (strncmp (e, "UCX", dtl_mode_env_len) == 0) {
            dtl_mode = DYAD_DTL_UCX;
        } else {
            if (debug) {
                fprintf (stderr,
                         "Invalid DTL mode provided through %s. \
                        Defaulting to UCX\n",
                         DYAD_DTL_MODE_ENV);
            }
            dtl_mode = DYAD_DTL_DEFAULT;
        }
    } else {
        dtl_mode = DYAD_DTL_DEFAULT;
    }
    if (debug)
        fprintf (stderr,
                 "DYAD_CORE: retrieved configuration from environment. Now "
                 "initializing DYAD\n");
    return dyad_init (debug,
                      check,
                      shared_storage,
                      key_depth,
                      key_bins,
                      service_mux,
                      kvs_namespace,
                      prod_managed_path,
                      cons_managed_path,
                      dtl_mode,
                      ctx);
}

dyad_rc_t dyad_produce (dyad_ctx_t* ctx, const char* fname)
{
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h) {
        return DYAD_RC_NOCTX;
    }
    // If the producer-managed path is NULL or empty, then the context is not
    // valid for a producer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->prod_managed_path == NULL || strlen (ctx->prod_managed_path) == 0) {
        return DYAD_RC_BADMANAGEDPATH;
    }
    // If the context is valid, call dyad_commit to perform
    // the producer operation
    return dyad_commit (ctx, fname);
}

// DYAD_CORE_FUNC_MODS dyad_rc_t dyad_kvs_lookup (const dyad_ctx_t* ctx,
//                                                const char* restrict kvs_topic,
//                                                uint32_t* owner_rank,
//                                                flux_future_t** f)
dyad_rc_t dyad_get_metadata (dyad_ctx_t* ctx,
                             const char* fname,
                             bool should_wait,
                             dyad_metadata_t** mdata)
{
    dyad_rc_t rc = DYAD_RC_OK;
    const size_t topic_len = PATH_MAX;
    char topic[PATH_MAX + 1];
    char upath[PATH_MAX];
    memset (topic, 0, topic_len + 1);
    memset (topic, '\0', topic_len + 1);
    memset (upath, 0, PATH_MAX);
    // Extract the path to the file specified by fname relative to the
    // producer-managed path
    // This relative path will be stored in upath
    DYAD_LOG_INFO (ctx, "Obtaining file path relative to consumer directory: %s", upath);
    if (!cmp_canonical_path_prefix (ctx->cons_managed_path, fname, upath, PATH_MAX)) {
        DYAD_LOG_INFO (ctx, "%s is not in the Consumer's managed path\n", fname);
        rc = DYAD_RC_UNTRACKED;
        goto get_metadata_done;
    }
    DYAD_LOG_INFO (ctx, "Generating KVS key: %s", topic);
    gen_path_key (upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
    rc = dyad_kvs_read (ctx, topic, fname, should_wait, mdata);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERR (ctx, "Could not read data from the KVS");
        goto get_metadata_done;
    }
    rc = DYAD_RC_OK;

get_metadata_done:
    if (DYAD_IS_ERROR (rc) && mdata != NULL && *mdata != NULL) {
        dyad_free_metadata (mdata);
    }
    return rc;
}

dyad_rc_t dyad_free_metadata (dyad_metadata_t** mdata)
{
    if (mdata == NULL || *mdata == NULL) {
        return DYAD_RC_OK;
    }
    if ((*mdata)->fpath != NULL)
        free ((*mdata)->fpath);
    free (*mdata);
    *mdata = NULL;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_consume (dyad_ctx_t* ctx, const char* fname)
{
    int rc = 0;
    int fd = -1;
    ssize_t file_size = -1;
    char* file_data = NULL;
    size_t data_len = 0ul;
    dyad_metadata_t* mdata = NULL;
    struct flock exclusive_lock;
    struct flock shared_lock;
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h) {
        return DYAD_RC_NOCTX;
    }
    // If the consumer-managed path is NULL or empty, then the context is not
    // valid for a consumer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->cons_managed_path == NULL || strlen (ctx->cons_managed_path) == 0) {
        return DYAD_RC_BADMANAGEDPATH;
    }
    // Set reenter to false to avoid recursively performing
    // DYAD operations
    ctx->reenter = false;
    fd = open (fname, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        DYAD_LOG_ERR (ctx, "Cannot create file (%s) for dyad_consume!\n", fname);
        return DYAD_RC_BADFIO;
    }
    rc = dyad_excl_flock (ctx, fd, &exclusive_lock);
    if (DYAD_IS_ERROR (rc)) {
        dyad_release_flock (ctx, fd, &exclusive_lock);
        goto consume_done;
    }
    if ((file_size = get_file_size (fd)) <= 0) {
        DYAD_LOG_INFO (ctx, "[node %u rank %u pid %d] File (%s with fd %d) is not fetched yet", \
                       ctx->node_idx, ctx->rank, ctx->pid, fname, fd);
        // Call dyad_fetch to get (and possibly wait on)
        // data from the Flux KVS
        rc = dyad_fetch (ctx, fname, &mdata);
        // If an error occured in dyad_fetch, log an error
        // and return the corresponding DYAD return code
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERR (ctx, "dyad_fetch failed!\n");
            dyad_release_flock (ctx, fd, &exclusive_lock);
            goto consume_done;
        }
        // If dyad_fetch was successful, but resp is still NULL,
        // then we need to skip data transfer.
        // This will most likely happend because shared_storage
        // is enabled
        if (mdata == NULL) {
            DYAD_LOG_INFO (ctx, "The KVS response is NULL! Skipping dyad_pull!\n");
            rc = DYAD_RC_OK;
            dyad_release_flock (ctx, fd, &exclusive_lock);
            goto consume_done;
        }

        // Call dyad_get_data to dispatch a RPC to the producer's Flux broker
        // and retrieve the data associated with the file
        rc = dyad_get_data (ctx, mdata, &file_data, &data_len);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERR (ctx, "dyad_get_data failed!\n");
            dyad_release_flock (ctx, fd, &exclusive_lock);
            goto consume_done;
        }

        // Call dyad_pull to fetch the data from the producer's
        // Flux broker
        rc = dyad_cons_store (ctx, mdata, fd, data_len, file_data);
        // Regardless if there was an error in dyad_pull,
        // free the KVS response object
        if (mdata != NULL) {
            dyad_free_metadata (&mdata);
        }
        // If an error occured in dyad_pull, log it
        // and return the corresponding DYAD return code
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERR (ctx, "dyad_cons_store failed!\n");
            dyad_release_flock (ctx, fd, &exclusive_lock);
            goto consume_done;
        };
        fsync (fd);
    }
    dyad_release_flock (ctx, fd, &exclusive_lock);

    if (close (fd) != 0) {
        rc = DYAD_RC_BADFIO;
        goto consume_done;
    }
    rc = DYAD_RC_OK;
consume_done:;
    if (file_data != NULL) {
        free ((void*) file_data);
    }
    // Set reenter to true to allow additional intercepting
    ctx->reenter = true;
    return rc;
}

dyad_rc_t dyad_finalize (dyad_ctx_t** ctx)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (ctx == NULL || *ctx == NULL) {
        rc = DYAD_RC_OK;
        goto finalize_region_finish;
    }
    dyad_perf_t* perf_handle = (*ctx)->perf_handle;
    DYAD_PERF_REGION_BEGIN (perf_handle, "dyad_finalize");
    dyad_dtl_finalize (&(*ctx)->dtl_handle);
    if ((*ctx)->h != NULL) {
        flux_close ((*ctx)->h);
        (*ctx)->h = NULL;
    }
    if ((*ctx)->kvs_namespace != NULL) {
        free ((*ctx)->kvs_namespace);
        (*ctx)->kvs_namespace = NULL;
    }
    if ((*ctx)->prod_managed_path != NULL) {
        free ((*ctx)->prod_managed_path);
        (*ctx)->prod_managed_path = NULL;
    }
    if ((*ctx)->cons_managed_path != NULL) {
        free ((*ctx)->cons_managed_path);
        (*ctx)->cons_managed_path = NULL;
    }
    free (*ctx);
    *ctx = NULL;
    rc = DYAD_RC_OK;
finalize_region_finish:
    DYAD_PERF_REGION_END (perf_handle, "dyad_finalize");
    dyad_perf_finalize (&perf_handle);
    return rc;
}

#if DYAD_SYNC_DIR
int dyad_sync_directory (dyad_ctx_t* restrict ctx, const char* restrict path)
{  // Flush new directory entry https://lwn.net/Articles/457671/
    char path_copy[PATH_MAX + 1];
    int odir_fd = -1;
    char* odir = NULL;
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
    return rc;
}
#endif
