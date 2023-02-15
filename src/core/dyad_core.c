#include "dyad_core.h"
#include "dyad_err.h"
#include "utils.h"
#include "murmur3.h"

#include <libgen.h>
#include <unistd.h>

#ifdef __cplusplus
#include <climits>
#include <cstring>
#else
#include <limits.h>
#include <string.h>
#endif

const struct dyad_ctx dyad_ctx_default = {
    NULL,  // h
    false, // debug
    false, // check
    false, // reenter
    true,  // initialized
    false, // shared_storage
    false, // sync_started
    3u,    // key_depth
    1024u, // key_bins
    0u,    // rank
    NULL,  // kvs_namespace
    NULL,  // prod_managed_path
    NULL   // cons_managed_path
};

static int gen_path_key (const char* str, char* path_key, const size_t len,
        const uint32_t depth, const uint32_t width)
{
    static const uint32_t seeds [10]
        = {104677u, 104681u, 104683u, 104693u, 104701u,
            104707u, 104711u, 104717u, 104723u, 104729u};

    uint32_t seed = 57u;
    uint32_t hash[4] = {0u}; // Output for the hash
    size_t cx = 0ul;
    int n = 0;

    if (path_key == NULL || len == 0ul) {
        return -1;
    }
    path_key [0] = '\0';

    for (uint32_t d = 0u; d < depth; d++)
    {
        seed += seeds [d % 10];
        // TODO add assert that str is not NULL
        MurmurHash3_x64_128 (str, strlen (str), seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key+cx, len-cx, "%x.", bin);
        cx += n;
        if (cx >= len || n < 0) {
            return -1;
        }
    }
    n = snprintf (path_key+cx, len-cx, "%s", str);
    if (cx + n >= len || n < 0) {
        return -1;
    }
    return 0;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_kvs_commit(dyad_ctx_t *ctx, flux_kvs_txn_t *txn,
        const char *upath)
#else
static inline int dyad_kvs_commit(dyad_ctx_t *ctx, flux_kvs_txn_t *txn,
        const char *upath)
#endif
{
    flux_future_t *f = NULL;
    // Commit the transaction to the Flux KVS
    printf("Committing data to KVS\n");
    f = flux_kvs_commit(ctx->h, ctx->kvs_namespace,
            0, txn);
    // If the commit failed, log an error and return DYAD_BADCOMMIT
    if (f == NULL)
    {
        DYAD_LOG_ERR(ctx, "Could not commit transaction to Flux KVS\n");
        return DYAD_BADCOMMIT;
    }
    // If the commit is pending, wait for it to complete
    printf("Wait for commit to complete\n");
    flux_future_wait_for(f, -1.0);
    printf("Destroy future for KVS commit\n");
    // Once the commit is complete, destroy the future and transaction
    flux_future_destroy(f);
    f = NULL;
    printf("Destroy transaction on KVS\n");
    flux_kvs_txn_destroy(txn);
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int publish_via_flux(dyad_ctx_t* ctx, const char *upath)
#else
static inline int publish_via_flux(dyad_ctx_t* ctx, const char *upath)
#endif
{
    printf("Initial publication setup\n");
    int ret = 0;
    const char* prod_managed_path = NULL;
    flux_kvs_txn_t *txn = NULL;
    const size_t topic_len = PATH_MAX;
    char topic[topic_len+1];
    memset(topic, 0, topic_len+1);
    prod_managed_path = ctx->prod_managed_path;
    printf("Creating KVS key\n");
    memset(topic, '\0', topic_len+1);
    // Generate the KVS key from the file path relative to
    // the producer-managed directory
    gen_path_key(
            upath,
            topic,
            topic_len,
            ctx->key_depth,
            ctx->key_bins
            );
    // If we don't have a valid Flux handle, we cannot continue.
    // So, print an error and return DYAD_NOCTX
    if (ctx->h == NULL)
    {
        fprintf (stderr, "No Flux handle found in publish_via_flux!\n");
        return DYAD_NOCTX;
    }
    // Crete and pack a Flux KVS transaction.
    // The transaction will contain a single key-value pair
    // with the previously generated key as the key and the
    // producer's rank as the value
    // TODO FLUX_LOG_INFO
    printf("Create KVS transaction\n");
    txn = flux_kvs_txn_create();
    if (txn == NULL)
    {
        DYAD_LOG_ERR(ctx, "Could not create Flux KVS transaction\n");
        return DYAD_FLUXFAIL;
    }
    printf("Pack producer rank into transaction\n");
    if (flux_kvs_txn_pack(txn, 0, topic, "i", ctx->rank) < 0)
    {
        DYAD_LOG_ERR(ctx, "Could not pack Flux KVS transaction\n");
        return DYAD_FLUXFAIL;
    }
    // Call dyad_kvs_commit to commit the transaction into the Flux KVS
    printf("Calling dyad_kvs_commit\n");
    ret = dyad_kvs_commit(ctx, txn, upath);
    // If dyad_kvs_commit failed, log an error and forward the return code
    if (ret < 0)
    {
        DYAD_LOG_ERR(ctx, "dyad_kvs_commit failed!\n");
        return ret;
    }
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_commit(dyad_ctx_t *ctx, const char *fname)
#else
static inline int dyad_commit(dyad_ctx_t *ctx, const char *fname)
#endif
{
    int rc = 0;
    char upath[PATH_MAX];
    memset(upath, 0, PATH_MAX);
    // If a context is not provided, print an error and return DYAD_NOCTX
    if (ctx == NULL)
    {
        fprintf (stderr, "DYAD context not provided to dyad_commit\n");
        return DYAD_NOCTX;
    }
    // Extract the path to the file specified by fname relative to the
    // producer-managed path
    // This relative path will be stored in upath
    if (!cmp_canonical_path_prefix (ctx->prod_managed_path, fname, upath, PATH_MAX))
    {
        DYAD_LOG_INFO (ctx, "%s is not in the Producer's managed path\n", fname);
        rc = DYAD_OK;
        goto commit_done;
    }
    // Call publish_via_flux to actually store information about the file into
    // the Flux KVS
    // Fence this call with reassignments of reenter so that, if intercepting
    // file I/O API calls, we will not get stuck in infinite recursion
    ctx->reenter = false;
    rc = publish_via_flux(ctx, upath);
    ctx->reenter = true;

commit_done:
    // If "check" is set and the operation was successful, set the
    // DYAD_CHECK_ENV environment variable to "ok"
    if (rc == DYAD_OK && (ctx && ctx->check))
    {
        setenv(DYAD_CHECK_ENV, "ok", 1);
    }
    return rc;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_kvs_lookup(dyad_ctx_t *ctx, const char* kvs_topic,
        uint32_t *owner_rank, flux_future_t **f)
#else
static inline int dyad_kvs_lookup(dyad_ctx_t *ctx, const char* kvs_topic,
        uint32_t *owner_rank, flux_future_t **f)
#endif
{
    int rc = -1;
    // Lookup information about the desired file (represented by kvs_topic)
    // from the Flux KVS. If there is no information, wait for it to be
    // made available
    *f = flux_kvs_lookup(ctx->h,
            ctx->kvs_namespace,
            FLUX_KVS_WAITCREATE,
            kvs_topic
            );
    // If the KVS lookup failed, log an error and return DYAD_BADLOOKUP
    if (*f == NULL)
    {
        DYAD_LOG_ERR (ctx, "KVS lookup failed!\n");
        return DYAD_BADLOOKUP;
    }
    // Extract the rank of the producer from the KVS response
    rc = flux_kvs_lookup_get_unpack(*f, "i", owner_rank);
    // If the extraction did not work, log an error and return DYAD_BADFETCH
    if (rc < 0)
    {
        DYAD_LOG_ERR(ctx, "Could not unpack owner's rank from KVS response\n");
        return DYAD_BADFETCH;
    }
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_fetch(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t **resp)
#else
static inline int dyad_fetch(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t **resp)
#endif
{
    int rc = 0;
    char upath[PATH_MAX];
    uint32_t owner_rank = 0;
    const size_t topic_len = PATH_MAX;
    char topic[topic_len+1];
    flux_future_t *f = NULL;
    memset(upath, 0, PATH_MAX);
    memset(topic, 0, topic_len+1);
    // Extract the path to the file specified by fname relative to the
    // consumer-managed path
    // This relative path will be stored in upath
    if (!cmp_canonical_path_prefix(ctx->cons_managed_path, fname, upath, PATH_MAX))
    {
        DYAD_LOG_INFO (ctx, "%s is not in the Consumer's managed path\n", fname);
        return DYAD_OK;
    }
    // Generate the KVS key from the file path relative to
    // the consumer-managed directory
    gen_path_key(
            upath,
            topic,
            topic_len,
            ctx->key_depth,
            ctx->key_bins
            );
    // Call dyad_kvs_lookup to retrieve infromation about the file
    // from the Flux KVS
    rc = dyad_kvs_lookup(ctx, topic, &owner_rank, &f);
    // If an error occured in dyad_kvs_lookup, log it and propagate the return code
    if (DYAD_IS_ERROR(rc))
    {
        DYAD_LOG_ERR(ctx, "dyad_kvs_lookup failed!\n");
        goto fetch_done;
    }
    // There are two cases where we do not want to perform file transfer:
    //   1. if the shared storage feature is enabled
    //   2. if the producer and consumer share the same rank
    // In either of these cases, skip the creation of the dyad_kvs_response_t object,
    // and return DYAD_OK. This will cause the file transfer step to be skipped
    if (ctx->shared_storage || (owner_rank == ctx->rank))
    {
        *resp = NULL;
        rc = DYAD_OK;
        goto fetch_done;
    }
    // Allocate and populate the dyad_kvs_response_t object.
    // If an error occurs, log it and return the appropriate
    // return code
    *resp = malloc(sizeof(struct dyad_kvs_response));
    if (*resp == NULL)
    {
        DYAD_LOG_ERR(ctx, "Cannot allocate a dyad_kvs_response_t object!\n");
        rc = DYAD_BADRESPONSE;
        goto fetch_done;
    }
    (*resp)->fpath = malloc(strlen(upath)+1);
    strncpy(
            (*resp)->fpath,
            upath,
            strlen(upath)+1
           );
    (*resp)->owner_rank = owner_rank;
fetch_done:;
    // Destroy the Flux future if needed
    if (f != NULL)
    {
        flux_future_destroy(f);
        f = NULL;
    }
    return rc;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_rpc_get(dyad_ctx_t *ctx, dyad_kvs_response_t *kvs_data,
        const char **file_data, int *file_len, flux_future_t **f)
#else
static inline int dyad_rpc_get(dyad_ctx_t *ctx, dyad_kvs_response_t *kvs_data,
        const char **file_data, int *file_len, flux_future_t **f)
#endif
{
    int rc = 0;
    // Create and send an RPC payload to the producer's Flux broker.
    // This payload will tell the broker to run the dyad.fetch function
    // with the upath specified by the data fetched from the KVS
    *f = flux_rpc_pack(ctx->h,
            "dyad.fetch",
            kvs_data->owner_rank,
            0,
            "{s:s}",
            "upath",
            kvs_data->fpath
            );
    // If the RPC dispatch failed, log an error and return DYAD_BADRPC
    if (*f == NULL)
    {
        DYAD_LOG_ERR(ctx, "Cannot send RPC to producer plugin!\n");
        return DYAD_BADRPC;
    }
    // Extract the file data from the RPC response
    rc = flux_rpc_get_raw(*f, (const void**) file_data, file_len);
    // If the extraction failed, log an error and return DYAD_BADRPC
    if (rc < 0)
    {
        DYAD_LOG_ERR(ctx, "Cannot get file back from plugin via RPC!\n");
        return DYAD_BADRPC;
    }
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
static int dyad_pull(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t *kvs_data)
#else
static inline int dyad_pull(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t *kvs_data)
#endif
{
    int rc = 0;
    const char *file_data = NULL;
    int file_len = 0;
    const char *odir = NULL;
    FILE *of = NULL;
    char file_path [PATH_MAX+1];
    char file_path_copy [PATH_MAX+1];
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    size_t written_len = 0;
    flux_future_t *f = NULL;
    memset(file_path, 0, PATH_MAX+1);
    memset(file_path_copy, 0, PATH_MAX+1);
    if (ctx == NULL)
    {
        fprintf (stderr, "DYAD context not provided to dyad_pull\n");
        return DYAD_NOCTX;
    }

    // Call dyad_rpc_get to dispatch a RPC to the producer's Flux broker
    // and retrieve the data associated with the file
    rc = dyad_rpc_get(ctx, kvs_data, &file_data, &file_len, &f);
    if (rc != DYAD_OK)
    {
        goto pull_done;
    }

    // Build the full path to the file being consumed
    strncpy (file_path, ctx->cons_managed_path, PATH_MAX-1);
    concat_str (file_path, kvs_data->fpath, "/", PATH_MAX);
    strncpy (file_path_copy, file_path, PATH_MAX); // dirname modifies the arg

    // Create the directory as needed
    // TODO: Need to be consistent with the mode at the source
    odir = dirname (file_path_copy);
    if ((strncmp (odir, ".", strlen(".")) != 0) &&
            (mkdir_as_needed (odir, m) < 0)) {
        DYAD_LOG_ERR(ctx, "Cannot create needed directories for pulled file\n");
        rc = DYAD_BADFIO;
        goto pull_done;
    }

    // Write the file contents to the location specified by the user
    of = fopen(file_path, "w");
    if (of == NULL)
    {
        DYAD_LOG_ERR(ctx, "Cannot open file %s\n", file_path);
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    written_len = fwrite(file_data, sizeof(char), (size_t) file_len, of);
    if (written_len != (size_t) file_len)
    {
        DYAD_LOG_ERR(ctx, "fwrite of pulled file failed!\n");
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    rc = fclose(of);

    if (rc != 0)
    {
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    rc = DYAD_OK;

pull_done:
    // Destroy the Flux future for the RPC, if needed
    if (f != NULL)
    {
        flux_future_destroy(f);
        f = NULL;
    }
    // If "check" is set and the operation was successful, set the
    // DYAD_CHECK_ENV environment variable to "ok"
    if (rc == DYAD_OK && (ctx && ctx->check))
        setenv (DYAD_CHECK_ENV, "ok", 1);
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static int dyad_free_kvs_response(dyad_kvs_response_t **resp)
{
    if (resp == NULL || *resp == NULL)
    {
        return DYAD_OK;
    }
    free((*resp)->fpath);
    free(*resp);
    *resp = NULL;
    return DYAD_OK;
}

int dyad_init(bool debug, bool check, bool shared_storage,
        unsigned int key_depth, unsigned int key_bins,
        const char *kvs_namespace, const char *prod_managed_path,
        const char *cons_managed_path, dyad_ctx_t **ctx)
{
    printf("In dyad_init\n");
    // If ctx is NULL, we won't be able to return a dyad_ctx_t
    // to the user. In that case, print an error and return
    // immediately with DYAD_NOCTX.
    if (ctx == NULL)
    {
        fprintf(stderr, "'ctx' argument to dyad_init is NULL! This prevents us from returning a dyad_ctx_t object!\n");
        return DYAD_NOCTX;
    }
    // Check if the actual dyad_ctx_t object is not NULL.
    // If it is not NULL, that means the dyad_ctx_t object
    // has either been allocated or fully initialized.
    // If it's initialized, simply print a message and
    // return DYAD_OK. Otherwise, initialize the dyad_ctx_t
    // object with dyad_ctx_default and return DYAD_OK.
    if (*ctx != NULL)
    {
        if ((*ctx)->initialized)
        {
            // TODO Indicate already initialized
            DPRINTF ((*ctx), "DYAD context already initialized\n");
        }
        else
        {
            **ctx = dyad_ctx_default;
        }
        return DYAD_OK;
    }
    printf("Allocating ctx\n");
    // Allocate the dyad_ctx_t object and make sure the allocation
    // worked successfully
    *ctx = (dyad_ctx_t*) malloc(sizeof(struct dyad_ctx));
    if (*ctx == NULL)
    {
        fprintf (stderr, "Could not allocate DYAD context!\n");
        return DYAD_NOCTX;
    }
    // Set the initial contents of the dyad_ctx_t object
    // to dyad_ctx_default.
    printf("Setting ctx values that don't need allocation\n");
    **ctx = dyad_ctx_default;
    // If neither managed path is provided, DYAD will not do anything.
    // So, simply print a warning and return DYAD_OK.
    if (prod_managed_path == NULL && cons_managed_path == NULL)
    {
        fprintf (stderr, "Warning: no managed path provided! DYAD will not do anything!\n");
        return DYAD_OK;
    }
    // Set the values in dyad_ctx_t that don't need allocation
    (*ctx)->debug = debug;
    (*ctx)->check = check;
    (*ctx)->shared_storage = shared_storage;
    (*ctx)->key_depth = key_depth;
    (*ctx)->key_bins = key_bins;
    // Open a Flux handle and store it in the dyad_ctx_t
    // object. If the open operation failed, return DYAD_FLUXFAIL
    printf("Opening Flux\n");
    (*ctx)->h = flux_open(NULL, 0);
    if ((*ctx)->h == NULL)
    {
        fprintf (stderr, "Could not open Flux handle!\n");
        return DYAD_FLUXFAIL;
    }
    // Get the rank of the Flux broker corresponding
    // to the handle. If this fails, return DYAD_FLUXFAIL
    printf("Getting Flux Rank\n");
    if (flux_get_rank((*ctx)->h, &((*ctx)->rank)) < 0)
    {
        fprintf(stderr, "Could not get Flux rank\n");
        FLUX_LOG_ERR ((*ctx)->h, "Could not get Flux rank!\n");
        return DYAD_FLUXFAIL;
    }
    // If the namespace is provided, copy it into the dyad_ctx_t object
    printf("Setting KVS namespace\n");
    if (kvs_namespace == NULL)
    {
        fprintf (stderr, "No KVS namespace provided!\n");
        // TODO see if we want a different return val
        return DYAD_NOCTX;
    }
    const size_t namespace_len = strlen(kvs_namespace);
    (*ctx)->kvs_namespace = (char*) malloc(namespace_len+1);
    if ((*ctx)->kvs_namespace == NULL)
    {
        fprintf (stderr, "Could not allocate buffer for KVS namespace!\n");
        free(*ctx);
        *ctx = NULL;
        return DYAD_NOCTX;
    }
    strncpy(
            (*ctx)->kvs_namespace,
            kvs_namespace,
            namespace_len+1
           );
    // If the producer-managed path is provided, copy it into
    // the dyad_ctx_t object
    printf("Setting prod_managed_path\n");
    if (prod_managed_path == NULL)
    {
        (*ctx)->prod_managed_path = NULL;
    }
    else
    {
        (*ctx)->prod_managed_path = (char*) malloc(strlen(prod_managed_path)+1);
        if ((*ctx)->prod_managed_path == NULL)
        {
            fprintf (stderr, "Could not allocate buffer for Producer managed path!\n");
            free((*ctx)->kvs_namespace);
            free(*ctx);
            *ctx = NULL;
            return DYAD_NOCTX;
        }
        strncpy(
                (*ctx)->prod_managed_path,
                prod_managed_path,
                strlen(prod_managed_path)+1
            );
    }
    // If the consumer-managed path is provided, copy it into
    // the dyad_ctx_t object
    printf("Setting cons_managed_path\n");
    if (cons_managed_path == NULL)
    {
        (*ctx)->cons_managed_path = NULL;
    }
    else
    {
        (*ctx)->cons_managed_path = (char*) malloc(strlen(cons_managed_path)+1);
        if ((*ctx)->cons_managed_path == NULL)
        {
            fprintf (stderr, "Could not allocate buffer for Consumer managed path!\n");
            free((*ctx)->kvs_namespace);
            free((*ctx)->prod_managed_path);
            free(*ctx);
            *ctx = NULL;
            return DYAD_NOCTX;
        }
        strncpy(
                (*ctx)->cons_managed_path,
                cons_managed_path,
                strlen(cons_managed_path)+1
            );
    }
    // Initialization is now complete!
    // Set reenter and initialized to indicate this.
    printf("Initialization complete\n");
    (*ctx)->reenter = true;
    (*ctx)->initialized = true;
    // TODO Print logging info
    return DYAD_OK;
}

int dyad_produce(dyad_ctx_t *ctx, const char *fname)
{
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h)
    {
        return DYAD_NOCTX;
    }
    // If the producer-managed path is NULL or empty, then the context is not valid
    // for a producer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->prod_managed_path == NULL || strlen(ctx->prod_managed_path) == 0)
    {
        return DYAD_BADMANAGEDPATH;
    }
    // If the context is valid, call dyad_commit to perform
    // the producer operation
    return dyad_commit(ctx, fname);
}

int dyad_consume(dyad_ctx_t *ctx, const char *fname)
{
    // If the context is not defined, then it is not valid.
    // So, return DYAD_NOCTX
    if (!ctx || !ctx->h)
    {
        return DYAD_NOCTX;
    }
    // If the consumer-managed path is NULL or empty, then the context is not valid
    // for a consumer operation. So, return DYAD_BADMANAGEDPATH
    if (ctx->cons_managed_path == NULL || strlen(ctx->cons_managed_path) == 0)
    {
        return DYAD_BADMANAGEDPATH;
    }
    // Set reenter to false to avoid recursively performing
    // DYAD operations
    ctx->reenter = false;
    // Call dyad_fetch to get (and possibly wait on)
    // data from the Flux KVS
    dyad_kvs_response_t *resp = NULL;
    rc = dyad_fetch(ctx, fname, &resp);
    // If an error occured in dyad_fetch, log an error
    // and return the corresponding DYAD return code
    if (DYAD_IS_ERROR(rc))
    {
        DYAD_LOG_ERR (ctx, "dyad_fetch failed!\n");
        // Set reenter to true to allow additional intercepting
        ctx->reenter = true;
        // No need to free 'resp' because it will always
        // be NULL if dyad_fetch doesn't return DYAD_OK
        return rc;
    }
    // If dyad_fetch was successful, but resp is still NULL,
    // then we need to skip data transfer.
    // This will most likely happend because shared_storage
    // is enabled
    if (resp == NULL)
    {
        DYAD_LOG_INFO (ctx, "The KVS response is NULL! Skipping dyad_pull!\n");
        // Set reenter to true to allow additional intercepting
        ctx->reenter = true;
        return DYAD_OK;
    }
    // Call dyad_pull to fetch the data from the producer's
    // Flux broker
    rc = dyad_pull(ctx, fname, resp);
    // Set reenter to true to allow additional intercepting
    ctx->reenter = true;
    // Regardless if there was an error in dyad_pull,
    // free the KVS response object
    dyad_free_kvs_response(&resp);
    // If an error occured in dyad_pull, log it
    // and return the corresponding DYAD return code
    if (DYAD_IS_ERROR(rc))
    {
        DYAD_LOG_ERR (ctx, "dyad_free_kvs_response failed!\n");
        return rc;
    };
    return DYAD_OK;
}

int dyad_finalize(dyad_ctx_t **ctx)
{
    if (ctx == NULL || *ctx == NULL)
    {
        return DYAD_OK;
    }
    if ((*ctx)->h != NULL)
    {
        flux_close((*ctx)->h);
        (*ctx)->h = NULL;
    }
    if ((*ctx)->kvs_namespace != NULL)
    {
        free((*ctx)->kvs_namespace);
        (*ctx)->kvs_namespace = NULL;
    }
    if ((*ctx)->prod_managed_path != NULL)
    {
        free((*ctx)->prod_managed_path);
        (*ctx)->prod_managed_path = NULL;
    }
    if ((*ctx)->cons_managed_path != NULL)
    {
        free((*ctx)->cons_managed_path);
        (*ctx)->cons_managed_path = NULL;
    }
    free(*ctx);
    *ctx = NULL;
    return DYAD_OK;
}

#if DYAD_SYNC_DIR
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_sync_directory(dyad_ctx_t *ctx, const char *path)
{ // Flush new directory entry https://lwn.net/Articles/457671/
    char path_copy [PATH_MAX+1];
    int odir_fd = -1;
    char *odir = NULL;
    bool reenter = false;
    int rc = 0;
    memset(path_copy, 0, PATH_MAX+1);

    strncpy (path_copy, path, PATH_MAX);
    odir = dirname (path_copy);

    reenter = ctx->reenter; // backup ctx->reenter
    if (ctx != NULL) ctx->reenter = false;

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
    if (ctx != NULL) ctx->reenter = reenter;
    return rc;
}
#endif
