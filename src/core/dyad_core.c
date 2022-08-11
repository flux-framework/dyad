// Also provides dyad_core.h
#include "dyad_open_dlwrap.h"
#include "dyad_err.h"
#include "utils.h"
#include "murmur3.h"

#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

const struct dyad_ctx dyad_ctx_default = {
    NULL,
    false,
    false,
    false,
    true,
    false,
    false,
    3u,
    1024u,
    0u,
    NULL,
    NULL
};

int gen_path_key (const char* str, char* path_key, const size_t len,
        const uint32_t depth, const uint32_t width)
{
    static const uint32_t seeds [10]
        = {104677, 104681, 104683, 104693, 104701,
            104707, 104711, 104717, 104723, 104729};

    uint32_t seed = 57;
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

int dyad_init(bool debug, bool check, bool shared_storage,
        unsigned int key_depth, unsigned int key_bins,
        const char *kvs_namespace, const char *prod_managed_path,
        const char *cons_managed_path, bool intercept, dyad_ctx_t **ctx)
{
    if (*ctx != NULL)
    {
        if ((*ctx)->initialized)
        {
            // TODO Indicate already initialized
        }
        else
        {
            **ctx = dyad_ctx_default;
        }
        return DYAD_OK;
    }
    *ctx = (dyad_ctx_t*) malloc(sizeof(dyad_ctx_t));
    if (*ctx == NULL)
    {
        // TODO Indicate error
        return DYAD_NOCTX;
    }
    **ctx = dyad_ctx_default;
    (*ctx)->debug = debug;
    (*ctx)->check = check;
    (*ctx)->shared_storage = shared_storage;
    (*ctx)->key_depth = key_depth;
    (*ctx)->key_bins = key_bins;
    (*ctx)->intercept = intercept;
    (*ctx)->kvs_namespace = (char*) malloc(strlen(kvs_namespace)+1);
    if ((*ctx)->kvs_namespace == NULL)
    {
        // TODO Indicate error
        free(*ctx);
        *ctx = NULL;
        return DYAD_NOCTX;
    }
    strncpy(
            (*ctx)->kvs_namespace,
            kvs_namespace,
            strlen(kvs_namespace)+1
           );
    (*ctx)->prod_managed_path = (char*) malloc(strlen(prod_managed_path)+1);
    if ((*ctx)->prod_managed_path == NULL)
    {
        // TODO Indicate error
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
    (*ctx)->cons_managed_path = (char*) malloc(strlen(cons_managed_path)+1);
    if ((*ctx)->cons_managed_path == NULL)
    {
        // TODO Indicate error
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
    (*ctx)->reenter = true;
    (*ctx)->h = flux_open(NULL, 0);
    if ((*ctx)->h == NULL)
    {
        // TODO Indicate error
        return DYAD_FLUXFAIL;
    }
    if (flux_get_rank((*ctx)->h, &((*ctx)->rank)) < 0)
    {
        // TODO Indicate error
        return DYAD_FLUXFAIL;
    }
    (*ctx)->initialized = true;
    // TODO Print logging info
    return DYAD_OK;
}

int dyad_produce(dyad_ctx_t *ctx, const char *fname)
{
    return dyad_commit(ctx, fname);
}

int dyad_consume(dyad_ctx_t *ctx, const char *fname)
{
    dyad_kvs_response_t *resp = NULL;
    int rc;
    rc = dyad_fetch(ctx, fname, &resp);
    if (rc != 0)
    {
        // TODO Indicate error
        if (resp != NULL)
        {
            dyad_free_kvs_response(resp);
        }
        return rc;
    }
    rc = dyad_pull(ctx, fname, resp);
    dyad_free_kvs_response(resp);
    if (rc != 0)
    {
        // TODO Indicate error
        return rc;
    };
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_kvs_commit(dyad_ctx_t *ctx, flux_kvs_txn_t *txn,
        const char *upath, flux_future_t **f)
{
    *f = flux_kvs_commit(ctx->h, ctx->kvs_namespace,
            0, txn);
    if (*f == NULL)
    {
        // TODO FLUX_LOG_ERR
        return DYAD_BADCOMMIT;
    }
    flux_future_wait_for(*f, -1.0);
    flux_future_destroy(*f);
    *f = NULL;
    flux_kvs_txn_destroy(txn);
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int publish_via_flux(dyad_ctx_t* ctx, const char *upath)
{
    const char *prod_managed_path = ctx->prod_managed_path;
    flux_kvs_txn_t *txn = NULL;
    flux_future_t *f = NULL;
    const size_t topic_len = PATH_MAX;
    char topic[topic_len+1];
    memset(topic, '\0', topic_len+1);
    gen_path_key(
            upath,
            topic,
            topic_len,
            ctx->key_depth,
            ctx->key_bins
            );
    if (ctx->h == NULL)
    {
        // TODO Set correct ret val
        return DYAD_NOCTX;
    }
    // TODO FLUX_LOG_INFO
    txn = flux_kvs_txn_create();
    if (txn == NULL)
    {
        // TODO Log err
        return DYAD_FLUXFAIL;
    }
    if (flux_kvs_txn_pack(txn, 0, topic, "i", ctx->rank) < 0)
    {
        // TODO Log err
        return DYAD_FLUXFAIL;
    }
    int ret = dyad_kvs_commit(ctx, txn, upath, &f);
    if (ret < 0)
    {
        // TODO log error
        return ret;
    }
    return DYAD_OK;
}

int dyad_commit(dyad_ctx_t *ctx, const char *fname)
{
    if (ctx == NULL)
    {
        // TODO log
        return DYAD_NOCTX;
    }
    int rc = -1;
    char upath[PATH_MAX] = {'\0'};
    if (!cmp_canonical_path_prefix (ctx->prod_managed_path, fname, upath, PATH_MAX))
    {
        // TODO log error
        rc = 0;
        goto commit_done;
    }
    ctx->reenter = false;
    rc = publish_via_flux(ctx, upath);
    ctx->reenter = true;

commit_done:
    if (rc == DYAD_OK && (ctx && ctx->check))
    {
        setenv(DYAD_CHECK_ENV, "ok", 1);
    }
    return rc;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_kvs_lookup(dyad_ctx_t *ctx, const char* kvs_topic, uint32_t *owner_rank)
{
    int rc = -1;
    flux_future_t *f = NULL;
    f = flux_kvs_lookup(ctx->h,
            ctx->kvs_namespace,
            FLUX_KVS_WAITCREATE,
            kvs_topic
            );
    if (f == NULL)
    {
        // TODO log
        return DYAD_BADLOOKUP;
    }
    rc = flux_kvs_lookup_get_unpack(f, "i", owner_rank);
    if (f != NULL)
    {
        flux_future_destroy(f);
        f = NULL;
    }
    if (rc < 0)
    {
        // TODO log
        return DYAD_BADFETCH;
    }
    return DYAD_OK;
}

int dyad_fetch(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t **resp)
{
    if (ctx == NULL)
    {
        // TODO log
        return DYAD_NOCTX;
    }
    int rc = -1;
    char upath[PATH_MAX] = {'\0'};
    if (!cmp_canonical_path_prefix(ctx->cons_managed_path, fname, upath, PATH_MAX))
    {
        // TODO log error
        return 0;
    }
    uint32_t owner_rank = 0;
    const size_t topic_len = PATH_MAX;
    char topic[topic_len+1];
    memset(topic, '\0', topic_len+1);
    gen_path_key(
            upath,
            topic,
            topic_len,
            ctx->key_depth,
            ctx->key_bins
            );
    if (ctx->h == NULL)
    {
        // TODO log
        return DYAD_NOCTX;
    }
    rc = dyad_kvs_lookup(ctx, topic, &owner_rank);
    if (rc != DYAD_OK)
    {
        // TODO log error
        return rc;
    }
    if (ctx->shared_storage || (owner_rank == ctx->rank))
    {
        return 0;
    }
    *resp = malloc(sizeof(struct dyad_kvs_response));
    if (*resp == NULL)
    {
        // TODO log error
        return DYAD_BADRESPONSE;
    }
    (*resp)->fpath = malloc(strlen(upath)+1);
    strncpy(
            (*resp)->fpath,
            upath,
            strlen(upath)+1
           );
    (*resp)->owner_rank = owner_rank;
    return DYAD_OK;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_rpc_get(dyad_ctx_t *ctx, dyad_kvs_response_t *kvs_data,
        const char **file_data, int *file_len)
{
    int rc = -1;
    flux_future_t *f = NULL;
    f = flux_rpc_pack(ctx->h,
            "dyad.fetch",
            kvs_data->owner_rank,
            0,
            "{s:s}",
            "upath",
            kvs_data->fpath
            );
    if (f == NULL)
    {
        // TODO log
        return DYAD_BADRPC;
    }
    rc = flux_rpc_get_raw(f, (const void**) file_data, file_len);
    if (f != NULL)
    {
        flux_future_destroy(f);
        f = NULL;
    }
    if (rc < 0)
    {
        // TODO log
        return DYAD_BADRPC;
    }
    return DYAD_OK;
}

int dyad_pull(dyad_ctx_t *ctx, const char* fname,
        dyad_kvs_response_t *kvs_data)
{
    if (ctx == NULL)
    {
        // TODO log
        return DYAD_NOCTX;
    }
    int rc = -1;
    const char *file_data = NULL;
    int file_len = 0;
    const char *odir = NULL;
    FILE *of = NULL;
    char file_path [PATH_MAX+1] = {'\0'};
    char file_path_copy [PATH_MAX+1] = {'\0'};

    rc = dyad_rpc_get(ctx, kvs_data, &file_data, &file_len);
    if (rc != DYAD_OK)
    {
        goto pull_done;
    }

    ctx->reenter = false;

    strncpy (file_path, ctx->cons_managed_path, PATH_MAX-1);
    concat_str (file_path, kvs_data->fpath, "/", PATH_MAX);
    strncpy (file_path_copy, file_path, PATH_MAX); // dirname modifies the arg

    // Create the directory as needed
    // TODO: Need to be consistent with the mode at the source
    odir = dirname (file_path_copy);
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if ((strncmp (odir, ".", strlen(".")) != 0) &&
            (mkdir_as_needed (odir, m) < 0)) {
        // TODO log that dirs could not be created
        rc = DYAD_BADFIO;
        goto pull_done;
    }

    of = dyad_fopen(ctx, file_path, "w");
    if (of == NULL)
    {
        // TODO log that file could not be opened
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    size_t written_len = fwrite(file_data, sizeof(char), (size_t) file_len, of);
    if (written_len != (size_t) file_len)
    {
        // TODO log that write did not work
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    rc = dyad_fclose(ctx, of);

    if (rc != 0)
    {
        rc = DYAD_BADFIO;
        goto pull_done;
    }
    rc = DYAD_OK;

pull_done:
    ctx->reenter = true;
    if (rc == DYAD_OK && (ctx && ctx->check))
        setenv (DYAD_CHECK_ENV, "ok", 1);
    return DYAD_OK;
}

int dyad_free_kvs_response(dyad_kvs_response_t *resp)
{
    if (resp == NULL)
    {
        return 0;
    }
    free(resp->fpath);
    free(resp);
    resp = NULL;
    return 0;
}

int dyad_finalize(dyad_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return 0;
    }
    flux_close(ctx->h);
    free(ctx->kvs_namespace);
    free(ctx->prod_managed_path);
    free(ctx->cons_managed_path);
    free(ctx);
    ctx = NULL;
    return 0;
}

#if DYAD_SYNC_DIR
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int dyad_sync_directory(dyad_ctx_t *ctx, const char *path)
{ // Flush new directory entry https://lwn.net/Articles/457671/
    char path_copy [PATH_MAX+1] = {'\0'};
    int odir_fd = -1;
    char *odir = NULL;
    bool reenter = false;
    int rc = 0;

    strncpy (path_copy, path, PATH_MAX);
    odir = dirname (path_copy);

    reenter = ctx->reenter; // backup ctx->reenter
    if (ctx != NULL) ctx->reenter = false;

    if ((odir_fd = dyad_open (ctx, odir, O_RDONLY)) < 0) {
        IPRINTF (ctx, "Cannot open the directory \"%s\"\n", odir);
        rc = -1;
    } else {
        if (fsync (odir_fd) < 0) {
            IPRINTF (ctx, "Cannot flush the directory \"%s\"\n", odir);
            rc = -1;
        }
        if (dyad_close (ctx, odir_fd) < 0) {
            IPRINTF (ctx, "Cannot close the directory \"%s\"\n", odir);
            rc = -1;
        }
    }
    if (ctx != NULL) ctx->reenter = reenter;
    return rc;
}
#endif
