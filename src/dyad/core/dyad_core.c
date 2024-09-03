#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/core/dyad_core_int.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/utils/murmur3.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>
#include <flux/core.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
#include <climits>
#include <cstring>
#else
#include <limits.h>
#include <linux/limits.h>
#include <string.h>
#endif

DYAD_DLL_EXPORTED int gen_path_key(const char *restrict str,
                                   char *restrict path_key, const size_t len,
                                   const uint32_t depth, const uint32_t width) {
  DYAD_C_FUNCTION_START();
  static const uint32_t seeds[10] = {104677u, 104681u, 104683u, 104693u,
                                     104701u, 104707u, 104711u, 104717u,
                                     104723u, 104729u};

  uint32_t seed = 57u;
  uint32_t hash[4] = {0u}; // Output for the hash
  size_t cx = 0ul;
  int n = 0;
  if (str == NULL || path_key == NULL || len == 0ul) {
    DYAD_C_FUNCTION_END();
    return -1;
  }
  size_t str_len = strlen(str);
  if (str_len == 0ul) {
    DYAD_C_FUNCTION_END();
    return -1;
  }
  const char *str_long = str;

  path_key[0] = '\0';

  // Just append the string so that it can be as large as 128 bytes.
  if (str_len < 128ul) {
    char buf[256] = {'\0'};
    memcpy(buf, str, str_len);
    memset(buf + str_len, '@', 128ul - str_len);
    buf[128u] = '\0';
    str_len = 128ul;
    str_long = buf;
  }

  for (uint32_t d = 0u; d < depth; d++) {
    seed += seeds[d % 10];
    MurmurHash3_x64_128(str_long, str_len, seed, hash);
    uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
    n = snprintf(path_key + cx, len - cx, "%x.", bin);
    cx += n;
    if (cx >= len || n < 0) {
      DYAD_C_FUNCTION_END();
      return -1;
    }
  }
  n = snprintf(path_key + cx, len - cx, "%s", str);
  // FIXME: cx + n >= len  fails for str_len > 256
  if (n < 0) {
    DYAD_C_FUNCTION_END();
    return -1;
  }
  DYAD_C_FUNCTION_UPDATE_STR("path_key", path_key);
  DYAD_C_FUNCTION_END();
  return 0;
}

static void future_cleanup_cb(flux_future_t *f, void *arg) {
  if (flux_future_get(f, NULL) < 0) {
    DYAD_LOG_STDERR("future_cleanup: future error detected with.%s", "");
  }
  flux_future_destroy(f);
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_kvs_commit(const dyad_ctx_t *restrict ctx,
                                              flux_kvs_txn_t *restrict txn) {
  DYAD_C_FUNCTION_START();
  flux_future_t *f = NULL;
  dyad_rc_t rc = DYAD_RC_OK;
  DYAD_LOG_INFO(ctx, "Committing transaction to KVS");
  // Commit the transaction to the Flux KVS
  f = flux_kvs_commit((flux_t *)ctx->h, ctx->kvs_namespace, 0, txn);
  // If the commit failed, log an error and return DYAD_BADCOMMIT
  if (f == NULL) {
    DYAD_LOG_ERROR(ctx, "Could not commit transaction to Flux KVS");
    rc = DYAD_RC_BADCOMMIT;
    goto kvs_commit_region_finish;
  }
  if (ctx->async_publish) {
    if (flux_future_then(f, -1, future_cleanup_cb, NULL) < 0) {
      DYAD_LOG_ERROR(ctx, "Error with flux_future_then");
    }
  } else {
    // If the commit is pending, wait for it to complete
    flux_future_wait_for(f, -1.0);
    // Once the commit is complete, destroy the future and transaction
    flux_future_destroy(f);
    f = NULL;
  }
  rc = DYAD_RC_OK;
kvs_commit_region_finish:;
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t publish_via_flux(const dyad_ctx_t *restrict ctx,
                                               const char *restrict upath) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", ctx->fname);
  DYAD_C_FUNCTION_UPDATE_STR("upath", upath);
  dyad_rc_t rc = DYAD_RC_OK;
  flux_kvs_txn_t *txn = NULL;
  const size_t topic_len = PATH_MAX;
  char topic[PATH_MAX + 1] = {'\0'};
  memset(topic, 0, topic_len + 1);
  memset(topic, '\0', topic_len + 1);
  // Generate the KVS key from the file path relative to
  // the producer-managed directory
  DYAD_LOG_INFO(ctx, "Generating KVS key from path (%s)", upath);
  gen_path_key(upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
  // Crete and pack a Flux KVS transaction.
  // The transaction will contain a single key-value pair
  // with the previously generated key as the key and the
  // producer's rank as the value
  DYAD_LOG_INFO(ctx, "Creating KVS transaction under the key %s", topic);
  txn = flux_kvs_txn_create();
  if (txn == NULL) {
    DYAD_LOG_ERROR(ctx, "Could not create Flux KVS transaction");
    rc = DYAD_RC_FLUXFAIL;
    goto publish_done;
  }
  if (flux_kvs_txn_pack(txn, 0, topic, "i", ctx->rank) < 0) {
    DYAD_LOG_ERROR(ctx, "Could not pack Flux KVS transaction");
    rc = DYAD_RC_FLUXFAIL;
    goto publish_done;
  }
  // Call dyad_kvs_commit to commit the transaction into the Flux KVS
  rc = dyad_kvs_commit(ctx, txn);
  // If dyad_kvs_commit failed, log an error and forward the return code
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "dyad_kvs_commit failed!");
    goto publish_done;
  }
  rc = DYAD_RC_OK;
publish_done:;
  if (txn != NULL) {
    flux_kvs_txn_destroy(txn);
  }
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_commit(dyad_ctx_t *restrict ctx,
                                        const char *restrict fname) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", ctx->fname);
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
      (strncmp(fname, DYAD_PATH_DELIM, ctx->delim_len) !=
       0)) { // fname is a relative path that is relative to the
             // prod_managed_path
    memcpy(upath, fname, strlen(fname));
  } else if (!cmp_canonical_path_prefix(ctx, true, fname, upath, PATH_MAX)) {
    // Extract the path to the file specified by fname relative to the
    // producer-managed path
    // This relative path will be stored in upath
    DYAD_LOG_INFO(ctx, "%s is not in the Producer's managed path", fname);
    rc = DYAD_RC_OK;
    goto commit_done;
  }
  DYAD_C_FUNCTION_UPDATE_STR("upath", upath);
  DYAD_LOG_INFO(ctx, "Obtained file path relative to producer directory: %s",
                upath);
  // Call publish_via_flux to actually store information about the file into
  // the Flux KVS
  // Fence this call with reassignments of reenter so that, if intercepting
  // file I/O API calls, we will not get stuck in infinite recursion
  ctx->reenter = false;
  rc = publish_via_flux(ctx, upath);
  ctx->reenter = true;

commit_done:;
  // If "check" is set and the operation was successful, set the
  // DYAD_CHECK_ENV environment variable to "ok"
  if (rc == DYAD_RC_OK && (ctx && ctx->check)) {
    setenv(DYAD_CHECK_ENV, "ok", 1);
  }
  DYAD_C_FUNCTION_END();
  return rc;
}

static void print_mdata(const dyad_ctx_t *restrict ctx,
                        const dyad_metadata_t *restrict mdata) {
  if (mdata == NULL) {
    DYAD_LOG_INFO(ctx, "Cannot print a NULL metadata object!");
  } else {
    DYAD_LOG_INFO(ctx, "Printing contents of DYAD Metadata object");
    DYAD_LOG_INFO(ctx, "fpath = %s", mdata->fpath);
    DYAD_LOG_INFO(ctx, "owner_rank = %u", mdata->owner_rank);
  }
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_kvs_read(const dyad_ctx_t *restrict ctx,
                                          const char *restrict topic,
                                          const char *restrict upath,
                                          bool should_wait,
                                          dyad_metadata_t **restrict mdata) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("upath", upath);
  dyad_rc_t rc = DYAD_RC_OK;
  int kvs_lookup_flags = 0;
  flux_future_t *f = NULL;
  if (mdata == NULL) {
    DYAD_LOG_ERROR(ctx, "Metadata double pointer is NULL. "
                        "Cannot correctly create metadata object");
    rc = DYAD_RC_NOTFOUND;
    goto kvs_read_end;
  }
  // Lookup information about the desired file (represented by kvs_topic)
  // from the Flux KVS. If there is no information, wait for it to be
  // made available
  if (should_wait)
    kvs_lookup_flags = FLUX_KVS_WAITCREATE;
  DYAD_LOG_INFO(ctx, "Retrieving information from KVS under the key %s", topic);
  f = flux_kvs_lookup((flux_t *)ctx->h, ctx->kvs_namespace, kvs_lookup_flags,
                      topic);
  // If the KVS lookup failed, log an error and return DYAD_BADLOOKUP
  if (f == NULL) {
    DYAD_LOG_ERROR(ctx, "KVS lookup failed!\n");
    rc = DYAD_RC_NOTFOUND;
    goto kvs_read_end;
  }
  // Extract the rank of the producer from the KVS response
  DYAD_LOG_INFO(ctx, "Building metadata object from KVS entry\n");
  if (*mdata != NULL) {
    DYAD_LOG_INFO(ctx,
                  "Metadata object is already allocated. Skipping allocation");
  } else {
    *mdata = (dyad_metadata_t *)malloc(sizeof(struct dyad_metadata));
    if (*mdata == NULL) {
      DYAD_LOG_ERROR(ctx, "Cannot allocate memory for metadata object");
      rc = DYAD_RC_SYSFAIL;
      goto kvs_read_end;
    }
  }
  size_t upath_len = strlen(upath);
  (*mdata)->fpath = (char *)malloc(upath_len + 1);
  if ((*mdata)->fpath == NULL) {
    DYAD_LOG_ERROR(ctx, "Cannot allocate memory for fpath in metadata object");
    rc = DYAD_RC_SYSFAIL;
    goto kvs_read_end;
  }
  memset((*mdata)->fpath, '\0', upath_len + 1);
  memcpy((*mdata)->fpath, upath, upath_len);
  rc = flux_kvs_lookup_get_unpack(f, "i", &((*mdata)->owner_rank));
  // If the extraction did not work, log an error and return DYAD_BADFETCH
  if (rc < 0) {
    DYAD_LOG_ERROR(ctx, "Could not unpack owner's rank from KVS response\n");
    rc = DYAD_RC_BADMETADATA;
    goto kvs_read_end;
  }
  DYAD_LOG_INFO(ctx, "Successfully created DYAD Metadata object");
  print_mdata(ctx, *mdata);
  DYAD_C_FUNCTION_UPDATE_STR("fpath", (*mdata)->fpath);
  DYAD_C_FUNCTION_UPDATE_INT("owner_rank", (*mdata)->owner_rank);
  rc = DYAD_RC_OK;

kvs_read_end:;
  if (DYAD_IS_ERROR(rc) && mdata != NULL && *mdata != NULL) {
    dyad_free_metadata(mdata);
  }
  if (f != NULL) {
    flux_future_destroy(f);
    f = NULL;
  }
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_fetch_metadata(
    const dyad_ctx_t *restrict ctx, const char *restrict fname,
    const char *restrict upath, dyad_metadata_t **restrict mdata) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", fname);
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
  DYAD_LOG_INFO(ctx, "Obtained file path relative to consumer directory: %s\n",
                upath);
  DYAD_C_FUNCTION_UPDATE_STR("upath", upath);
  // Generate the KVS key from the file path relative to
  // the consumer-managed directory
  gen_path_key(upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
  DYAD_LOG_INFO(ctx, "Generated KVS key for consumer: %s\n", topic);
  // Call dyad_kvs_read to retrieve infromation about the file
  // from the Flux KVS
  rc = dyad_kvs_read(ctx, topic, upath, true, mdata);
  // If an error occured in dyad_kvs_read, log it and propagate the return
  // code
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "dyad_kvs_read failed!\n");
    goto fetch_done;
  }
  // There are two cases where we do not want to perform file transfer:
  //   1. if the shared storage feature is enabled
  //   2. if the producer and consumer share the same rank
  // In either of these cases, skip the creation of the dyad_kvs_response_t
  // object, and return DYAD_OK. This will cause the file transfer step to be
  // skipped
  DYAD_C_FUNCTION_UPDATE_INT("owner_rank", (*mdata)->owner_rank);
  DYAD_C_FUNCTION_UPDATE_INT("node_idx", ctx->node_idx);
  if (((*mdata)->owner_rank / ctx->service_mux) == ctx->node_idx) {
    DYAD_LOG_INFO(
        ctx,
        "Either shared-storage is indicated or the producer rank (%u) is the"
        " same as the consumer rank (%u)",
        (*mdata)->owner_rank, ctx->rank);
    if (mdata != NULL && *mdata != NULL) {
      dyad_free_metadata(mdata);
    }
    rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_UPDATE_INT("is_local", 1);
    goto fetch_done;
  }
  DYAD_C_FUNCTION_UPDATE_INT("is_local", 0);
  rc = DYAD_RC_OK;

fetch_done:;
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_get_data(const dyad_ctx_t *restrict ctx,
                                          const dyad_metadata_t *restrict mdata,
                                          char **restrict file_data,
                                          size_t *restrict file_len) {
  DYAD_C_FUNCTION_START();
  dyad_rc_t rc = DYAD_RC_OK;
  flux_future_t *f = NULL;
  json_t *rpc_payload = NULL;
  DYAD_LOG_INFO(ctx, "Packing payload for RPC to DYAD module");
  DYAD_C_FUNCTION_UPDATE_INT("owner_rank", mdata->owner_rank);
  DYAD_C_FUNCTION_UPDATE_STR("fpath", mdata->fpath);
  rc = ctx->dtl_handle->rpc_pack(ctx, mdata->fpath, mdata->owner_rank,
                                 &rpc_payload);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Cannot create JSON payload for Flux RPC to "
                        "DYAD module\n");
    goto get_done;
  }
  DYAD_LOG_INFO(ctx, "Sending payload for RPC to DYAD module");
  f = flux_rpc_pack((flux_t *)ctx->h, DYAD_DTL_RPC_NAME, mdata->owner_rank,
                    FLUX_RPC_STREAMING, "o", rpc_payload);
  if (f == NULL) {
    DYAD_LOG_ERROR(ctx, "Cannot send RPC to producer module\n");
    rc = DYAD_RC_BADRPC;
    goto get_done;
  }
  DYAD_LOG_INFO(ctx, "Receive RPC response from DYAD module");
  rc = ctx->dtl_handle->rpc_recv_response(ctx, f);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Cannot receive and/or parse the RPC response\n");
    goto get_done;
  }
  DYAD_LOG_INFO(ctx, "Establish DTL connection with DYAD module");
  rc = ctx->dtl_handle->establish_connection(ctx);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx,
                   "Cannot establish connection with DYAD module on broker "
                   "%u\n",
                   mdata->owner_rank);
    goto get_done;
  }
  DYAD_LOG_INFO(ctx, "Receive file data via DTL");
  rc = ctx->dtl_handle->recv(ctx, (void **)file_data, file_len);
  DYAD_LOG_INFO(ctx, "Close DTL connection with DYAD module");
  ctx->dtl_handle->close_connection(ctx);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Cannot receive data from producer module\n");
    goto get_done;
  }
  DYAD_C_FUNCTION_UPDATE_INT("file_len", *file_len);

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
  DYAD_LOG_INFO(
      ctx, "Wait for end-of-stream message from module (current RC = %d)\n",
      rc);
  if (rc != DYAD_RC_RPC_FINISHED && rc != DYAD_RC_BADRPC) {
    if (!(flux_rpc_get(f, NULL) < 0 && errno == ENODATA)) {
      DYAD_LOG_ERROR(ctx,
                     "An error occured at end of getting data! Either the "
                     "module sent too many responses, or the module "
                     "failed with a bad error (errno = %d)\n",
                     errno);
      rc = DYAD_RC_BADRPC;
    }
  }
#ifdef DYAD_ENABLE_UCX_RMA
  ctx->dtl_handle->get_buffer(ctx, 0, (void **)file_data);
  ssize_t read_len = 0l;
  memcpy(&read_len, *file_data, sizeof(read_len));
  if (read_len < 0l) {
    *file_len = 0ul;
    DYAD_LOG_DEBUG(ctx, "Not able to read from %s file", mdata->fpath);
    rc = DYAD_RC_BADFIO;
  } else {
    *file_len = (size_t)read_len;
  }
  *file_data = ((char *)*file_data) + sizeof(read_len);
  DYAD_LOG_INFO(ctx, "Read %zd bytes from %s file", *file_len, mdata->fpath);
#endif
  DYAD_LOG_INFO(ctx, "Destroy the Flux future for the RPC\n");
  flux_future_destroy(f);
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_CORE_FUNC_MODS dyad_rc_t dyad_cons_store(
    const dyad_ctx_t *restrict ctx, const dyad_metadata_t *restrict mdata,
    int fd, const size_t data_len, char *restrict file_data) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_INT("fd", fd);
  dyad_rc_t rc = DYAD_RC_OK;
  const char *odir = NULL;
  char file_path[PATH_MAX + 1] = {'\0'};
  char file_path_copy[PATH_MAX + 1] = {'\0'};
  mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
  size_t written_len = 0;
  memset(file_path, 0, PATH_MAX + 1);
  memset(file_path_copy, 0, PATH_MAX + 1);

  // Build the full path to the file being consumed
  strncpy(file_path, ctx->cons_managed_path, PATH_MAX - 1);
  concat_str(file_path, mdata->fpath, "/", PATH_MAX);
  memcpy(file_path_copy, file_path, PATH_MAX); // dirname modifies the arg
  DYAD_C_FUNCTION_UPDATE_STR("cons_managed_path", ctx->cons_managed_path);
  DYAD_C_FUNCTION_UPDATE_STR("fpath", mdata->fpath);
  DYAD_C_FUNCTION_UPDATE_STR("file_path_copy", file_path_copy);

  DYAD_LOG_INFO(ctx, "Saving retrieved data to %s\n", file_path);
  // Create the directory as needed
  // TODO: Need to be consistent with the mode at the source
  odir = dirname(file_path_copy);
  if ((strncmp(odir, ".", strlen(".")) != 0) &&
      (mkdir_as_needed(odir, m) < 0)) {
    DYAD_LOG_ERROR(ctx, "Cannot create needed directories for pulled file\n");
    rc = DYAD_RC_BADFIO;
    goto pull_done;
  }

  // Write the file contents to the location specified by the user
  written_len = write(fd, file_data, data_len);
  if (written_len != data_len) {
    DYAD_LOG_ERROR(ctx, "cons store write of pulled file failed!\n");
    rc = DYAD_RC_BADFIO;
    goto pull_done;
  }
  DYAD_C_FUNCTION_UPDATE_INT("data_len", data_len);
  rc = DYAD_RC_OK;

pull_done:;
  // If "check" is set and the operation was successful, set the
  // DYAD_CHECK_ENV environment variable to "ok"
  if (rc == DYAD_RC_OK && (ctx && ctx->check))
    setenv(DYAD_CHECK_ENV, "ok", 1);
  DYAD_C_FUNCTION_END();
  return rc;
}

dyad_rc_t dyad_produce(dyad_ctx_t *restrict ctx, const char *restrict fname) {
  DYAD_C_FUNCTION_START();
  ctx->fname = fname;
  DYAD_C_FUNCTION_UPDATE_STR("fname", ctx->fname);
  DYAD_LOG_DEBUG(ctx, "Executing dyad_produce");
  dyad_rc_t rc = DYAD_RC_OK;
  // If the context is not defined, then it is not valid.
  // So, return DYAD_NOCTX
  if (!ctx || !ctx->h) {
    DYAD_LOG_ERROR(ctx, "No CTX found in dyad_produce");
    rc = DYAD_RC_NOCTX;
    goto produce_done;
  }
  // If the producer-managed path is NULL or empty, then the context is not
  // valid for a producer operation. So, return DYAD_BADMANAGEDPATH
  if (ctx->prod_managed_path == NULL) {
    DYAD_LOG_ERROR(ctx, "No or empty producer managed path was found %s",
                   ctx->prod_managed_path);
    rc = DYAD_RC_BADMANAGEDPATH;
    goto produce_done;
  }
  // If the context is valid, call dyad_commit to perform
  // the producer operation
  rc = dyad_commit(ctx, fname);
produce_done:;
  DYAD_C_FUNCTION_END();
  return rc;
}

/** This function is coupled with Python API. This populates `mdata' which
 * is used by `dyad_consume_w_metadata ()'
 */
dyad_rc_t dyad_get_metadata(dyad_ctx_t *restrict ctx,
                            const char *restrict fname, bool should_wait,
                            dyad_metadata_t **restrict mdata) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", fname);
  DYAD_C_FUNCTION_UPDATE_INT("should_wait", should_wait);
  dyad_rc_t rc = DYAD_RC_OK;

#if 0
    if (fname == NULL || strlen (fname) > PATH_MAX) {
        rc = DYAD_RC_SYSFAIL;
        goto get_metadata_done;
    }
#endif
  const size_t fname_len = strlen(fname);
  char upath[PATH_MAX + 1] = {'\0'};

  DYAD_LOG_INFO(ctx, "Obtaining file path relative to consumer directory: %s",
                upath);

  if (fname_len == 0ul) {
    rc = DYAD_RC_BADFIO;
    goto get_metadata_done;
  }
  if (ctx->relative_to_managed_path &&
      (strncmp(fname, DYAD_PATH_DELIM, ctx->delim_len) !=
       0)) { // fname is a relative path that is relative to the
             // cons_managed_path
    memcpy(upath, fname, fname_len);
  } else if (!cmp_canonical_path_prefix(ctx, false, fname, upath, PATH_MAX)) {
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
  ctx->reenter = false;
  DYAD_C_FUNCTION_UPDATE_STR("upath", upath);

  // check if file exist locally, if so skip kvs
  int fd = open(fname, O_RDONLY);
  if (fd != -1) {
    close(fd);
    if (mdata == NULL) {
      DYAD_LOG_ERROR(ctx, "Metadata double pointer is NULL. "
                          "Cannot correctly create metadata object");
      rc = DYAD_RC_NOTFOUND;
      goto get_metadata_done;
    }
    if (*mdata != NULL) {
      DYAD_LOG_INFO(
          ctx, "Metadata object is already allocated. Skipping allocation");
    } else {
      *mdata = (dyad_metadata_t *)malloc(sizeof(struct dyad_metadata));
      if (*mdata == NULL) {
        DYAD_LOG_ERROR(ctx, "Cannot allocate memory for metadata object");
        rc = DYAD_RC_SYSFAIL;
        goto get_metadata_done;
      }
    }
    (*mdata)->fpath = (char *)malloc(fname_len + 1);
    if ((*mdata)->fpath == NULL) {
      DYAD_LOG_ERROR(ctx,
                     "Cannot allocate memory for fpath in metadata object");
      rc = DYAD_RC_SYSFAIL;
      goto get_metadata_done;
    }
    memset((*mdata)->fpath, '\0', fname_len + 1);
    memcpy((*mdata)->fpath, fname, fname_len);
    (*mdata)->owner_rank = ctx->rank;
    rc = DYAD_RC_OK;
    goto get_metadata_done;
  }

  const size_t topic_len = PATH_MAX;
  char topic[PATH_MAX + 1] = {'\0'};
  DYAD_LOG_INFO(ctx, "Generating KVS key: %s", topic);
  gen_path_key(upath, topic, topic_len, ctx->key_depth, ctx->key_bins);
  rc = dyad_kvs_read(ctx, topic, upath, should_wait, mdata);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Could not read data from the KVS");
    goto get_metadata_done;
  }
  rc = DYAD_RC_OK;

get_metadata_done:;
  if (DYAD_IS_ERROR(rc) && mdata != NULL && *mdata != NULL) {
    dyad_free_metadata(mdata);
  }
  ctx->reenter = true;
  DYAD_C_FUNCTION_END();
  return rc;
}

dyad_rc_t dyad_free_metadata(dyad_metadata_t **mdata) {
  DYAD_C_FUNCTION_START();
  if (mdata == NULL || *mdata == NULL) {
    return DYAD_RC_OK;
  }
  if ((*mdata)->fpath != NULL)
    free((*mdata)->fpath);
  free(*mdata);
  *mdata = NULL;
  DYAD_C_FUNCTION_END();
  return DYAD_RC_OK;
}

dyad_rc_t dyad_consume(dyad_ctx_t *restrict ctx, const char *restrict fname) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", fname);
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

  if (ctx->relative_to_managed_path && (strlen(fname) > 0ul) &&
      (strncmp(fname, DYAD_PATH_DELIM, ctx->delim_len) !=
       0)) { // fname is a relative path that is relative to the
             // cons_managed_path
    memcpy(upath, fname, strlen(fname));
  } else if (!cmp_canonical_path_prefix(ctx, false, fname, upath, PATH_MAX)) {
    // Extract the path to the file specified by fname relative to the
    // consumer-managed path
    // This relative path will be stored in upath
    // DYAD_LOG_TRACE (ctx, "%s is not in the Consumer's managed path\n",
    // fname);
    rc = DYAD_RC_OK;
    goto consume_close;
  }
  ctx->reenter = false;

  lock_fd = open(fname, O_RDWR | O_CREAT, 0666);
  if (lock_fd == -1) {
    // This could be a system file on which users have no write permission
    DYAD_LOG_ERROR(ctx, "Cannot create file (%s) for dyad_consume!\n", fname);
    rc = DYAD_RC_BADFIO;
    goto consume_close;
  }
  rc = dyad_excl_flock(ctx, lock_fd, &exclusive_lock);
  if (DYAD_IS_ERROR(rc)) {
    dyad_release_flock(ctx, lock_fd, &exclusive_lock);
    goto consume_done;
  }
  file_size = get_file_size(lock_fd);
  if (ctx->shared_storage) {
    dyad_release_flock(ctx, lock_fd, &exclusive_lock);
    if (!ctx->use_fs_locks || file_size <= 0) {
      // as file size was zero that means consumer won the lock first so has to
      // wait for kvs. or we cannot use file lock based synchronization as it
      // does not work with the files managed by c++ fstream.
      rc = dyad_fetch_metadata(ctx, fname, upath, &mdata);
      if (DYAD_IS_ERROR(rc)) {
        DYAD_LOG_ERROR(ctx,
                       "dyad_fetch_metadata failed fore shared storage!\n");
        goto consume_done;
      }
    }
  } else {
    if (file_size <= 0) {
      DYAD_LOG_INFO(ctx,
                    "[node %u rank %u pid %d] File (%s with lock_fd %d) is not "
                    "fetched yet",
                    ctx->node_idx, ctx->rank, ctx->pid, fname, lock_fd);
      // Call dyad_fetch to get (and possibly wait on)
      // data from the Flux KVS
      rc = dyad_fetch_metadata(ctx, fname, upath, &mdata);
      // If an error occured in dyad_fetch_metadata, log an error
      // and return the corresponding DYAD return code
      if (DYAD_IS_ERROR(rc)) {
        DYAD_LOG_ERROR(ctx, "dyad_fetch_metadata failed!\n");
        dyad_release_flock(ctx, lock_fd, &exclusive_lock);
        goto consume_done;
      }
      // If dyad_fetch_metadata was successful, but mdata is still NULL,
      // then we need to skip data transfer.
      // This is either because producer and consumer share storage
      // or because the file is not on the managed directory.
      if (mdata == NULL) {
        DYAD_LOG_INFO(ctx, "File '%s' is local!\n", fname);
        rc = DYAD_RC_OK;
        dyad_release_flock(ctx, lock_fd, &exclusive_lock);
        goto consume_done;
      }

      // Call dyad_get_data to dispatch a RPC to the producer's Flux broker
      // and retrieve the data associated with the file
      rc = dyad_get_data(ctx, mdata, &file_data, &data_len);
      if (DYAD_IS_ERROR(rc)) {
        DYAD_LOG_ERROR(ctx, "dyad_get_data failed!\n");
        dyad_release_flock(ctx, lock_fd, &exclusive_lock);
        goto consume_done;
      }
      DYAD_C_FUNCTION_UPDATE_INT("data_len", data_len);
      io_fd = open(fname, O_WRONLY);
      DYAD_C_FUNCTION_UPDATE_INT("io_fd", io_fd);
      if (io_fd == -1) {
        DYAD_LOG_ERROR(
            ctx, "Cannot open file (%s) in write mode for dyad_consume!\n",
            fname);
        rc = DYAD_RC_BADFIO;
        goto consume_close;
      }
      // Call dyad_pull to fetch the data from the producer's
      // Flux broker
      rc = dyad_cons_store(ctx, mdata, io_fd, data_len, file_data);
      // Regardless if there was an error in dyad_pull,
      // free the KVS response object
      if (mdata != NULL) {
        dyad_free_metadata(&mdata);
      }
      if (close(io_fd) != 0) {
        rc = DYAD_RC_BADFIO;
        dyad_release_flock(ctx, lock_fd, &exclusive_lock);
        goto consume_done;
      }
      // If an error occured in dyad_pull, log it
      // and return the corresponding DYAD return code
      if (DYAD_IS_ERROR(rc)) {
        DYAD_LOG_ERROR(ctx, "dyad_cons_store failed!\n");
        dyad_release_flock(ctx, lock_fd, &exclusive_lock);
        goto consume_done;
      };
    }
    dyad_release_flock(ctx, lock_fd, &exclusive_lock);
  }
  DYAD_C_FUNCTION_UPDATE_INT("file_size", file_size);
consume_done:;
  if (close(lock_fd) != 0) {
    rc = DYAD_RC_BADFIO;
  }
  if (file_data != NULL) {
    ctx->dtl_handle->return_buffer(ctx, (void **)&file_data);
  }
  // Set reenter to true to allow additional intercepting
consume_close:;
  ctx->reenter = true;
  DYAD_C_FUNCTION_END();
  return rc;
}

dyad_rc_t dyad_consume_w_metadata(dyad_ctx_t *restrict ctx, const char *fname,
                                  const dyad_metadata_t *restrict mdata) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("fname", fname);
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
    DYAD_LOG_INFO(ctx, "File '%s' is local!\n", fname);
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
  lock_fd = open(fname, O_RDWR | O_CREAT, 0666);
  DYAD_C_FUNCTION_UPDATE_INT("lock_fd", lock_fd);
  if (lock_fd == -1) {
    DYAD_LOG_ERROR(
        ctx, "Cannot create file (%s) for dyad_consume_w_metadata!\n", fname);
    rc = DYAD_RC_BADFIO;
    goto consume_close;
  }
  rc = dyad_excl_flock(ctx, lock_fd, &exclusive_lock);
  if (DYAD_IS_ERROR(rc)) {
    dyad_release_flock(ctx, lock_fd, &exclusive_lock);
    goto consume_close;
  }
  if ((file_size = get_file_size(lock_fd)) <= 0) {
    DYAD_LOG_INFO(
        ctx, "[node %u rank %u pid %d] File (%s with fd %d) is not fetched yet",
        ctx->node_idx, ctx->rank, ctx->pid, fname, lock_fd);

    // Call dyad_get_data to dispatch a RPC to the producer's Flux broker
    // and retrieve the data associated with the file
    rc = dyad_get_data(ctx, mdata, &file_data, &data_len);
    if (DYAD_IS_ERROR(rc)) {
      DYAD_LOG_ERROR(ctx, "dyad_get_data failed!\n");
      dyad_release_flock(ctx, lock_fd, &exclusive_lock);
      goto consume_done;
    }
    DYAD_C_FUNCTION_UPDATE_INT("data_len", data_len);
    io_fd = open(fname, O_WRONLY);
    DYAD_C_FUNCTION_UPDATE_INT("io_fd", io_fd);
    if (io_fd == -1) {
      DYAD_LOG_ERROR(ctx,
                     "Cannot open file (%s) in write mode for dyad_consume!\n",
                     fname);
      rc = DYAD_RC_BADFIO;
      goto consume_close;
    }
    // Call dyad_pull to fetch the data from the producer's
    // Flux broker
    rc = dyad_cons_store(ctx, mdata, io_fd, data_len, file_data);

    if (close(io_fd) != 0) {
      rc = DYAD_RC_BADFIO;
      dyad_release_flock(ctx, lock_fd, &exclusive_lock);
      goto consume_done;
    }
    // If an error occured in dyad_pull, log it
    // and return the corresponding DYAD return code
    if (DYAD_IS_ERROR(rc)) {
      DYAD_LOG_ERROR(ctx, "dyad_cons_store failed!\n");
      dyad_release_flock(ctx, io_fd, &exclusive_lock);
      goto consume_done;
    };
  }
  dyad_release_flock(ctx, lock_fd, &exclusive_lock);
  DYAD_C_FUNCTION_UPDATE_INT("file_size", file_size);

  if (close(lock_fd) != 0) {
    rc = DYAD_RC_BADFIO;
    goto consume_done;
  }
  rc = DYAD_RC_OK;
consume_done:;
  if (file_data != NULL) {
    ctx->dtl_handle->return_buffer(ctx, (void **)&file_data);
  }
consume_close:;
  // Set reenter to true to allow additional intercepting
  ctx->reenter = true;
  DYAD_C_FUNCTION_END();
  return rc;
}

#if DYAD_SYNC_DIR
int dyad_sync_directory(dyad_ctx_t *restrict ctx, const char *restrict path) {
  DYAD_C_FUNCTION_START();
  DYAD_C_FUNCTION_UPDATE_STR("path", path);
  // Flush new directory entry https://lwn.net/Articles/457671/
  char path_copy[PATH_MAX + 1] = {'\0'};
  int odir_fd = -1;
  char *odir = NULL;
  bool reenter = false;
  int rc = 0;
  memset(path_copy, 0, PATH_MAX + 1);

  strncpy(path_copy, path, PATH_MAX);
  odir = dirname(path_copy);

  reenter = ctx->reenter; // backup ctx->reenter
  if (ctx != NULL)
    ctx->reenter = false;

  if ((odir_fd = open(odir, O_RDONLY)) < 0) {
    IPRINTF(ctx, "Cannot open the directory \"%s\"\n", odir);
    rc = -1;
  } else {
    if (fsync(odir_fd) < 0) {
      IPRINTF(ctx, "Cannot flush the directory \"%s\"\n", odir);
      rc = -1;
    }
    if (close(odir_fd) < 0) {
      IPRINTF(ctx, "Cannot close the directory \"%s\"\n", odir);
      rc = -1;
    }
  }
  if (ctx != NULL)
    ctx->reenter = reenter;
  DYAD_C_FUNCTION_END();
  return rc;
}
#endif
