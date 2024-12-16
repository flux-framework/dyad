#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
// #include <dyad/core/dyad_core_int.h>
#include <dyad/core/dyad_ctx.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/utils/utils.h>
#include <flux/core.h>

#ifdef __cplusplus
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <errno.h>

// Note:
// To ensure we don't have multiple initialization, we need the following:
// 1) The DYAD context (ctx below) must be static
// 2) The DYAD context should be on the heap (done w/ malloc in dyad_init)
static __thread dyad_ctx_t *ctx = NULL;

const struct dyad_ctx dyad_ctx_default = {
    // Internal
    NULL,  // h
    NULL,  // dtl_handle
    NULL,  // fname
    false, // use_fs_locks
    NULL,  // prod_real_path
    NULL,  // cons_real_path
    0u,    // prod_managed_len
    0u,    // cons_managed_len
    0u,    // prod_real_len
    0u,    // cons_real_len
    0u,    // prod_managed_hash
    0u,    // cons_managed_hash
    0u,    // prod_real_hash
    0u,    // cons_real_hash
    0u,    // delim_len
    // User facing
    false, // debug
    false, // check
    false, // reenter
    true,  // initialized
    false, // shared_storage
    false, // async_publish
    false, // fsync_write
    3u,    // key_depth
    1024u, // key_bins
    0u,    // rank
    1u,    // service_mux
    0u,    // node_idx
    -1,    // pid
    NULL,  // kvs_namespace
    NULL,  // prod_managed_path
    NULL,  // cons_managed_path
    false  // relative_to_managed_path
};

DYAD_DLL_EXPORTED dyad_ctx_t *dyad_ctx_get() { return ctx; }

DYAD_DLL_EXPORTED void dyad_ctx_init(const dyad_dtl_comm_mode_t dtl_comm_mode,
                                     void *flux_handle) {
#if DYAD_PROFILER == 3
  DFTRACER_C_FINI();
#endif
  DYAD_C_FUNCTION_START();
  dyad_rc_t rc = DYAD_RC_OK;

  rc = dyad_init_env(dtl_comm_mode, flux_handle);

  if (DYAD_IS_ERROR(rc)) {
    fprintf(stderr, "Failed to initialize DYAD (code = %d)", rc);
    if (ctx != NULL) {
      ctx->initialized = false;
      ctx->reenter = false;
    }
    DYAD_C_FUNCTION_END();
    return;
  } else {
    DYAD_LOG_INFO(ctx, "DYAD Initialized on loading using env variables");
  }
  DYAD_C_FUNCTION_END();
}

DYAD_DLL_EXPORTED void dyad_ctx_fini() {
  DYAD_C_FUNCTION_START();
  if (ctx == NULL) {
    DYAD_C_FUNCTION_END();
    goto dyad_wrapper_fini_done;
  }
  dyad_finalize();
dyad_wrapper_fini_done:;
  DYAD_C_FUNCTION_END();
#if DYAD_PROFILER == 3
  DFTRACER_C_FINI();
#endif
}

dyad_rc_t dyad_clear();

DYAD_DLL_EXPORTED
dyad_rc_t dyad_init(bool debug, bool check, bool shared_storage, bool reinit,
                    bool async_publish, bool fsync_write,
                    unsigned int key_depth, unsigned int key_bins,
                    unsigned int service_mux, const char *kvs_namespace,
                    const char *prod_managed_path,
                    const char *cons_managed_path,
                    bool relative_to_managed_path, const char *dtl_mode_str,
                    const dyad_dtl_comm_mode_t dtl_comm_mode,
                    void *flux_handle) {
  DYAD_LOGGER_INIT();
  unsigned my_rank = 0u;
  size_t namespace_len = 0ul;

#ifdef DYAD_PROFILER_DFTRACER
  const char *file_prefix = getenv(DFTRACER_LOG_FILE);
  if (file_prefix == NULL)
    file_prefix = "./dyad_";
  char log_file[4096] = {'\0'};
  sprintf(log_file, "%s_core-%d.pfw", file_prefix, getpid());
  DFTRACER_C_INIT_NO_BIND(log_file, NULL, NULL);
#endif
  DYAD_C_FUNCTION_START();
  dyad_rc_t rc = DYAD_RC_OK;
  // Check if the actual dyad_ctx_t object is not NULL.
  // If it is not NULL, that means the dyad_ctx_t object
  // has either been allocated or fully initialized.
  // If it's initialized, simply print a message and
  // return DYAD_OK.
  if (!reinit && (ctx != NULL)) {
    if ((ctx)->initialized) {
      DYAD_LOG_DEBUG((ctx), "DYAD context already initialized\n");
      rc = DYAD_RC_OK;
      goto init_region_finish;
    }
  } else {
    if (reinit)
      dyad_finalize();
    // Allocate the dyad_ctx_t object and make sure the allocation
    // worked successfully
    ctx = (dyad_ctx_t *)malloc(sizeof(struct dyad_ctx));
    if (ctx == NULL) {
      fprintf(stderr, "Could not allocate DYAD context!\n");
      rc = DYAD_RC_NOCTX;
      goto init_region_finish;
    }
  }
  // Set the initial contents of the dyad_ctx_t object to dyad_ctx_default.
  *ctx = dyad_ctx_default;
  // If neither managed path is provided, DYAD will not do anything.
  // So, simply print a warning and return DYAD_OK.
  if (prod_managed_path == NULL && cons_managed_path == NULL) {
    fprintf(stderr, "Warning: no managed path provided! DYAD will not do "
                    "anything!\n");
    rc = DYAD_RC_OK;
    goto init_region_finish;
  }
#ifdef DYAD_PATH_DELIM
  if (DYAD_PATH_DELIM == NULL || strlen(DYAD_PATH_DELIM) == 0ul) {
    fprintf(stderr, "Invalid DYAD_PATH_DELIM defined in 'dyad_config.h'!\n");
    rc = DYAD_RC_NOCTX;
    goto init_region_failed;
  }
  ctx->delim_len = strlen(DYAD_PATH_DELIM);
#else
  fprintf(stderr, "DYAD_PATH_DELIM is not defined in 'dyad_config.h'!\n");
  rc = DYAD_RC_NOCTX;
  goto init_region_failed;
#endif
  // Set the values in dyad_ctx_t that don't need allocation
  ctx->debug = debug;
  ctx->check = check;
  ctx->shared_storage = shared_storage;
  ctx->async_publish = async_publish;
  ctx->fsync_write = fsync_write;
  ctx->key_depth = key_depth;
  ctx->key_bins = key_bins;

  // Open a Flux handle and store it in the dyad_ctx_t
  // object. If the open operation failed, return DYAD_FLUXFAIL
  // In case of module, we use the flux handle passed to mod_main()
  // instead of getting it by flux_open()

  if (flux_handle != NULL) {
    ctx->h = (flux_t *)flux_handle;
  } else {
    ctx->h = flux_open(NULL, 0);
  }
  if (ctx->h == NULL) {
    fprintf(stderr, "Could not open Flux handle!\n");
    rc = DYAD_RC_FLUXFAIL;
    goto init_region_finish;
  }

  // Get the rank of the Flux broker corresponding
  // to the handle. If this fails, return DYAD_FLUXFAIL
  DYAD_LOG_INFO(ctx, "DYAD_CORE: getting Flux rank");
  if (flux_get_rank(ctx->h, &(ctx->rank)) < 0) {
    DYAD_LOG_INFO(ctx, "Could not get Flux rank!\n");
    rc = DYAD_RC_FLUXFAIL;
    goto init_region_finish;
  }
  my_rank = ctx->rank;
  ctx->service_mux = (service_mux < 1u) ? 1u : service_mux;
  ctx->node_idx = ctx->rank / ctx->service_mux;
  ctx->pid = getpid();
  if (my_rank == 0) {
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: debug %s",
                  ctx->debug ? "true" : "false");
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: shared_storage %s",
                  ctx->shared_storage ? "true" : "false");
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: async_publish %s",
                  ctx->async_publish ? "true" : "false");
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: fsync_write %s",
                  ctx->fsync_write ? "true" : "false");
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: kvs key depth %u", ctx->key_depth);
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: kvs key bins %u", ctx->key_bins);
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: broker rank %u", my_rank);
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: pid %u", ctx->pid);
  }

  // If the namespace is provided, copy it into the dyad_ctx_t object
  DYAD_LOG_INFO(ctx, "DYAD_CORE: saving KVS namespace");
  if (kvs_namespace == NULL) {
    DYAD_LOG_ERROR(ctx, "No KVS namespace provided!\n");
    // TODO see if we want a different return val
    goto init_region_failed;
  }
  namespace_len = strlen(kvs_namespace);
  ctx->kvs_namespace = (char *)calloc(namespace_len + 1, sizeof(char));
  if (ctx->kvs_namespace == NULL) {
    DYAD_LOG_ERROR(ctx, "Could not allocate buffer for KVS namespace!\n");
    goto init_region_failed;
  }
  strncpy(ctx->kvs_namespace, kvs_namespace, namespace_len + 1);
  if (my_rank == 0) {
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: kvs_namespace %s", ctx->kvs_namespace);
  }

  // Initialize the DTL based on the value of dtl_mode
  // If an error occurs, log it and return an error
  DYAD_LOG_INFO(ctx, "DYAD_CORE: inintializing DYAD DTL %s", dtl_mode_str);
  rc = dyad_set_and_init_dtl_mode(dtl_mode_str, dtl_comm_mode);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Cannot initialize the DTL %s\n", dtl_mode_str);
    goto init_region_failed;
  }
  if (my_rank == 0) {
    DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: dtl_mode %s", dtl_mode_str);
  }

  // If the producer-managed path is provided, copy it into the dyad_ctx_t
  // object
  if (dyad_set_prod_path(prod_managed_path) != DYAD_RC_OK) {
    goto init_region_failed;
  }

  // If the consumer-managed path is provided, copy it into the dyad_ctx_t
  // object
  if (dyad_set_cons_path(cons_managed_path) != DYAD_RC_OK) {
    goto init_region_failed;
  }

  ctx->relative_to_managed_path = relative_to_managed_path;
  DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: relative_to_managed_path %s",
                ctx->relative_to_managed_path ? "true" : "false");

  DYAD_C_FUNCTION_UPDATE_STR("prod_managed_path", ctx->prod_managed_path);
  DYAD_C_FUNCTION_UPDATE_STR("cons_managed_path", ctx->cons_managed_path);
  ctx->use_fs_locks =
      true; // This is default value except for streams which dont have a fp.
  // TODO Print logging info
  rc = DYAD_RC_OK;
  // TODO: Add folder option here.
#ifndef DYAD_LOGGER_NO_LOG
  char log_file_name[4096] = {'\0'};
  char err_file_name[4096] = {'\0'};
  mkdir_as_needed("logs", (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID));
  sprintf(log_file_name, "logs/dyad_core_%d.out", ctx->pid);
  sprintf(err_file_name, "logs/dyad_core_%d.err", ctx->pid);
  DYAD_LOG_STDERR_REDIRECT(err_file_name);
  DYAD_LOG_STDERR_REDIRECT(log_file_name);
#endif // DYAD_LOGGER_NO_LOG
  // Initialization is now complete!
  ctx->initialized = true;
  goto init_region_finish;

init_region_failed:;
  dyad_clear();
  // Set the initial contents of the dyad_ctx_t object to dyad_ctx_default.
  *ctx = dyad_ctx_default;
  if (rc == DYAD_RC_NOCTX) {
    ctx->initialized = false;
    ctx->reenter = false;
  }
  return DYAD_RC_NOCTX;

init_region_finish:;
  ctx->reenter = true;
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t
dyad_init_env(const dyad_dtl_comm_mode_t dtl_comm_mode, void *flux_handle) {
  DYAD_C_FUNCTION_START();
  const char *e = NULL;
  bool debug = false;
  bool check = false;
  bool shared_storage = false;
  bool reinit = false;
  bool async_publish = false;
  bool fsync_write = false;
  bool relative_to_managed_path = false;
  unsigned int key_depth = 0u;
  unsigned int key_bins = 0u;
  unsigned int service_mux = 1u;
  const char *kvs_namespace = NULL;
  const char *prod_managed_path = NULL;
  const char *cons_managed_path = NULL;
  const char *dtl_mode = NULL;

  if ((e = getenv(DYAD_SYNC_DEBUG_ENV))) {
    debug = true;
  } else {
    debug = false;
  }

  if (debug) {
    DYAD_LOG_STDERR("DYAD_CORE: Initializing with environment "
                    "variables\n");
  }

  if ((e = getenv(DYAD_SYNC_CHECK_ENV))) {
    check = true;
  } else {
    check = false;
  }

  if ((e = getenv(DYAD_SHARED_STORAGE_ENV))) {
    shared_storage = true;
  } else {
    shared_storage = false;
  }

  if ((e = getenv(DYAD_REINIT_ENV))) {
    reinit = true;
  } else {
    reinit = false;
  }

  if ((e = getenv(DYAD_ASYNC_PUBLISH_ENV))) {
    async_publish = true;
  } else {
    async_publish = false;
  }

  if ((e = getenv(DYAD_FSYNC_WRITE_ENV))) {
    fsync_write = true;
  } else {
    fsync_write = false;
  }

  if ((e = getenv(DYAD_KEY_DEPTH_ENV))) {
    key_depth = atoi(e);
  } else {
    key_depth = 3;
  }

  if ((e = getenv(DYAD_KEY_BINS_ENV))) {
    key_bins = atoi(e);
  } else {
    key_bins = 1024;
  }

  if ((e = getenv(DYAD_SERVICE_MUX_ENV))) {
    service_mux = atoi(e);
  } else {
    service_mux = 1u;
  }

  if ((e = getenv(DYAD_KVS_NAMESPACE_ENV))) {
    kvs_namespace = e;
  } else {
    kvs_namespace = NULL;
  }

  if ((e = getenv(DYAD_PATH_CONSUMER_ENV))) {
    cons_managed_path = e;
  } else {
    cons_managed_path = NULL;
  }

  if ((e = getenv(DYAD_PATH_PRODUCER_ENV))) {
    prod_managed_path = e;
  } else {
    prod_managed_path = NULL;
  }

  if ((e = getenv(DYAD_PATH_RELATIVE_ENV))) {
    relative_to_managed_path = true;
  } else {
    relative_to_managed_path = false;
  }

  if ((e = getenv(DYAD_DTL_MODE_ENV))) {
    dtl_mode = e;
  } else {
    dtl_mode = dyad_dtl_mode_name[DYAD_DTL_DEFAULT];
    DYAD_LOG_STDERR("%s is not set. Defaulting to %s\n", DYAD_DTL_MODE_ENV,
                    dtl_mode);
  }
  if (debug) {
    DYAD_LOG_STDERR("DYAD_CORE: retrieved configuration from environment. "
                    "Now initializing DYAD\n");
  }
  dyad_rc_t rc =
      dyad_init(debug, check, shared_storage, reinit, async_publish,
                fsync_write, key_depth, key_bins, service_mux, kvs_namespace,
                prod_managed_path, cons_managed_path, relative_to_managed_path,
                dtl_mode, dtl_comm_mode, flux_handle);
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_set_and_init_dtl_mode(
    const char *dtl_name, dyad_dtl_comm_mode_t dtl_comm_mode) {
  int rc = DYAD_RC_OK;
  size_t dtl_name_len = 0ul;
  dyad_dtl_mode_t dtl_mode = DYAD_DTL_DEFAULT;

  DYAD_C_FUNCTION_START();

  if (!dtl_name) {
    return DYAD_RC_BADDTLMODE;
  }

  dtl_name_len = strlen(dtl_name);
  if (strncmp(dtl_name, dyad_dtl_mode_name[DYAD_DTL_DEFAULT], dtl_name_len) ==
      0) {
    dtl_mode = DYAD_DTL_DEFAULT;
  } else if (strncmp(dtl_name, dyad_dtl_mode_name[DYAD_DTL_UCX],
                     dtl_name_len) == 0) {
    dtl_mode = DYAD_DTL_UCX;
  } else if (strncmp(dtl_name, dyad_dtl_mode_name[DYAD_DTL_FLUX_RPC],
                     dtl_name_len) == 0) {
    dtl_mode = DYAD_DTL_FLUX_RPC;
  } else {
    DYAD_LOG_STDERR("Invalid env %s = %s.\n", DYAD_DTL_MODE_ENV, dtl_name);
    return DYAD_RC_BADDTLMODE;
  }

  DYAD_LOG_INFO(ctx, "DYAD_CORE: DYAD DTL mode to initialize: %s",
                dyad_dtl_mode_name[dtl_mode]);

  // If the ctx->dtl_handle is already null, it will just return success
  rc = dyad_dtl_finalize(ctx);
  if (DYAD_IS_ERROR(rc)) {
    goto set_and_init_dtl_mode_region_finish;
  }

  rc = dyad_dtl_init(ctx, dtl_mode, dtl_comm_mode, ctx->debug);
  if (DYAD_IS_ERROR(rc)) {
    DYAD_LOG_ERROR(ctx, "Cannot initialize the DTL %s\n",
                   dyad_dtl_mode_name[dtl_mode]);
  }

set_and_init_dtl_mode_region_finish:;
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_set_prod_path(const char *prod_managed_path) {
  dyad_rc_t rc = DYAD_RC_OK;
  bool reenter_backup = false;
  size_t prod_path_len = 0ul;
  size_t prod_realpath_len = 0ul;
  char prod_real_path[PATH_MAX + 1] = {'\0'};

  if (!ctx) {
    DYAD_LOG_STDERR("Empty Producer managed path to allocate!\n");
    rc = DYAD_RC_NOCTX;
    goto set_prod_path_region_finish;
  } else {
    reenter_backup = ctx->reenter;
    ctx->reenter = false;
  }
  DYAD_C_FUNCTION_START();

  if (ctx->prod_managed_path) {
    free(ctx->prod_managed_path);
    ctx->prod_managed_path = NULL;
    ctx->prod_managed_len = 0u;
    ctx->prod_managed_hash = 0u;
  }

  if (ctx->prod_real_path) {
    free(ctx->prod_real_path);
    ctx->prod_real_path = NULL;
    ctx->prod_real_len = 0u;
    ctx->prod_real_hash = 0u;
  }

  DYAD_LOG_INFO(ctx, "DYAD_CORE: saving producer path");
  if (prod_managed_path == NULL) {
    DYAD_LOG_INFO(ctx, "DYAD_CORE: producer path is not provided");
    ctx->prod_managed_path = NULL;
    ctx->prod_managed_len = 0u;
    ctx->prod_managed_hash = 0u;
    ctx->prod_real_path = NULL;
    ctx->prod_real_len = 0u;
    ctx->prod_real_hash = 0u;
  } else {
    if ((prod_path_len = strlen(prod_managed_path)) == 0ul) {
      DYAD_LOG_ERROR(ctx, "Empty Producer managed path to allocate!\n");
      rc = DYAD_RC_BADMANAGEDPATH;
      goto set_prod_path_region_failed;
    }
    ctx->prod_managed_path = (char *)calloc(prod_path_len + 1, sizeof(char));
    if (ctx->prod_managed_path == NULL) {
      DYAD_LOG_ERROR(ctx,
                     "Could not allocate buffer for Producer managed path!\n");
      rc = DYAD_RC_SYSFAIL;
      goto set_prod_path_region_failed;
    }
    if (!memcpy(ctx->prod_managed_path, prod_managed_path, prod_path_len + 1)) {
      DYAD_LOG_ERROR(ctx, "Could not copy Producer managed path!\n");
      rc = DYAD_RC_SYSFAIL;
      goto set_prod_path_region_failed;
    }
    ctx->prod_managed_len = prod_path_len;
    ctx->prod_managed_hash = hash_str(ctx->prod_managed_path, DYAD_SEED);
    if (ctx->prod_managed_hash == 0u) {
      DYAD_LOG_ERROR(ctx, "Could not hash Producer managed path!\n");
      rc = DYAD_RC_BADMANAGEDPATH;
      goto set_prod_path_region_failed;
    }

    if (!realpath(prod_managed_path, prod_real_path)) {
      DYAD_LOG_DEBUG(ctx, "Could not get Producer real path!\n");
      // perror ("Could not get Producer real path!\n");
      //  This is not necessarily an error. This can happen when the environment
      //  variables for both managed paths are set but consumer does not create
      //  the managed path of producer or vice versa. `realpath()' fails if a
      //  path does not exist.
    } else {
      prod_realpath_len = strlen(prod_real_path);
    }

    // If the realpath is the same, we turn off checking against it as it is
    // redundant
    if (prod_realpath_len == 0u ||
        strncmp(prod_managed_path, prod_real_path, prod_path_len) == 0) {
      DYAD_LOG_INFO(ctx, "DYAD_CORE: producer real path is redundant");
      ctx->prod_real_path = NULL;
      ctx->prod_real_len = 0u;
      ctx->prod_real_hash = 0u;
    } else {
      ctx->prod_real_path = (char *)calloc(prod_realpath_len + 1, sizeof(char));
      if (ctx->prod_real_path == NULL || prod_realpath_len == 0ul) {
        DYAD_LOG_ERROR(
            ctx, "Could not allocate buffer for Producer managed path!\n");
        rc = DYAD_RC_SYSFAIL;
        goto set_prod_path_region_failed;
      }

      if (!memcpy(ctx->prod_real_path, prod_real_path, prod_realpath_len)) {
        DYAD_LOG_ERROR(ctx, "Could not copy Producer real path!\n");
        rc = DYAD_RC_SYSFAIL;
        goto set_prod_path_region_failed;
      }
      ctx->prod_real_path[prod_realpath_len] = '\0';
      ctx->prod_real_len = prod_realpath_len;
      ctx->prod_real_hash = hash_str(ctx->prod_real_path, DYAD_SEED);
      if (ctx->prod_real_hash == 0u) {
        DYAD_LOG_ERROR(ctx, "Could not hash Producer real path!\n");
        rc = DYAD_RC_BADMANAGEDPATH;
        goto set_prod_path_region_failed;
      }
    }

    if (ctx->rank == 0) {
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_managed_path %s",
                    ctx->prod_managed_path);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_managed_len  %u",
                    ctx->prod_managed_len);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_managed_hash %u",
                    ctx->prod_managed_hash);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_real_path %s",
                    ctx->prod_real_path);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_real_len  %u",
                    ctx->prod_real_len);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: prod_real_hash %u",
                    ctx->prod_real_hash);
    }
  }
set_prod_path_region_failed:;

set_prod_path_region_finish:;
  if (ctx) {
    ctx->reenter = reenter_backup;
  }
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_set_cons_path(const char *cons_managed_path) {
  dyad_rc_t rc = DYAD_RC_OK;
  bool reenter_backup = false;
  size_t cons_path_len = 0ul;
  size_t cons_realpath_len = 0ul;
  char cons_real_path[PATH_MAX + 1] = {'\0'};

  if (!ctx) {
    DYAD_LOG_STDERR("Empty Consumer managed path to allocate!\n");
    rc = DYAD_RC_NOCTX;
    goto set_cons_path_region_finish;
  } else {
    reenter_backup = ctx->reenter;
    ctx->reenter = false;
  }
  DYAD_C_FUNCTION_START();

  if (ctx->cons_managed_path) {
    free(ctx->cons_managed_path);
    ctx->cons_managed_path = NULL;
    ctx->cons_managed_len = 0u;
    ctx->cons_managed_hash = 0u;
  }

  if (ctx->cons_real_path) {
    free(ctx->cons_real_path);
    ctx->cons_real_path = NULL;
    ctx->cons_real_len = 0u;
    ctx->cons_real_hash = 0u;
  }

  DYAD_LOG_INFO(ctx, "DYAD_CORE: saving consucer path");
  if (cons_managed_path == NULL) {
    DYAD_LOG_INFO(ctx, "DYAD_CORE: consucer path is not provided");
    ctx->cons_managed_path = NULL;
    ctx->cons_managed_len = 0u;
    ctx->cons_managed_hash = 0u;
    ctx->cons_real_path = NULL;
    ctx->cons_real_len = 0u;
    ctx->cons_real_hash = 0u;
  } else {
    if ((cons_path_len = strlen(cons_managed_path)) == 0ul) {
      DYAD_LOG_ERROR(ctx, "Empty Consumer managed path to allocate!\n");
      rc = DYAD_RC_BADMANAGEDPATH;
      goto set_cons_path_region_failed;
    }
    ctx->cons_managed_path = (char *)calloc(cons_path_len + 1, sizeof(char));
    if (ctx->cons_managed_path == NULL) {
      DYAD_LOG_ERROR(ctx,
                     "Could not allocate buffer for Consumer managed path!\n");
      rc = DYAD_RC_SYSFAIL;
      goto set_cons_path_region_failed;
    }
    if (!memcpy(ctx->cons_managed_path, cons_managed_path, cons_path_len + 1)) {
      DYAD_LOG_ERROR(ctx, "Could not copy Consumer managed path!\n");
      rc = DYAD_RC_SYSFAIL;
      goto set_cons_path_region_failed;
    }
    ctx->cons_managed_len = cons_path_len;
    ctx->cons_managed_hash = hash_str(ctx->cons_managed_path, DYAD_SEED);
    if (ctx->cons_managed_hash == 0u) {
      DYAD_LOG_ERROR(ctx, "Could not hash Consumer managed path!\n");
      rc = DYAD_RC_BADMANAGEDPATH;
      goto set_cons_path_region_failed;
    }

    if (!realpath(cons_managed_path, cons_real_path)) {
      DYAD_LOG_DEBUG(ctx, "Could not get Consumer real path!\n");
      // perror ("Could not get Consumer real path!\n");
      //  This is not necessarily an error. This can happen when the environment
      //  variables for both managed paths are set but consumer does not create
      //  the managed path of consucer or vice versa. `realpath()' fails if a
      //  path does not exist.
    } else {
      cons_realpath_len = strlen(cons_real_path);
    }

    // If the realpath is the same, we turn off checking against it as it is
    // redundant
    if (cons_realpath_len == 0u ||
        strncmp(cons_managed_path, cons_real_path, cons_path_len) == 0) {
      DYAD_LOG_INFO(ctx, "DYAD_CORE: consucer real path is redundant");
      ctx->cons_real_path = NULL;
      ctx->cons_real_len = 0u;
      ctx->cons_real_hash = 0u;
    } else {
      ctx->cons_real_path = (char *)calloc(cons_realpath_len + 1, sizeof(char));
      if (ctx->cons_real_path == NULL || cons_realpath_len == 0ul) {
        DYAD_LOG_ERROR(
            ctx, "Could not allocate buffer for Consumer managed path!\n");
        rc = DYAD_RC_SYSFAIL;
        goto set_cons_path_region_failed;
      }

      if (!memcpy(ctx->cons_real_path, cons_real_path, cons_realpath_len)) {
        DYAD_LOG_ERROR(ctx, "Could not copy Consumer real path!\n");
        rc = DYAD_RC_SYSFAIL;
        goto set_cons_path_region_failed;
      }
      ctx->cons_real_path[cons_realpath_len] = '\0';
      ctx->cons_real_len = cons_realpath_len;
      ctx->cons_real_hash = hash_str(ctx->cons_real_path, DYAD_SEED);
      if (ctx->cons_real_hash == 0u) {
        DYAD_LOG_ERROR(ctx, "Could not hash Consumer real path!\n");
        rc = DYAD_RC_BADMANAGEDPATH;
        goto set_cons_path_region_failed;
      }
    }

    if (ctx->rank == 0) {
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_managed_path %s",
                    ctx->cons_managed_path);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_managed_len  %u",
                    ctx->cons_managed_len);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_managed_hash %u",
                    ctx->cons_managed_hash);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_real_path %s",
                    ctx->cons_real_path);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_real_len  %u",
                    ctx->cons_real_len);
      DYAD_LOG_INFO(ctx, "DYAD_CORE INIT: cons_real_hash %u",
                    ctx->cons_real_hash);
    }
  }
set_cons_path_region_failed:;

set_cons_path_region_finish:;
  if (ctx) {
    ctx->reenter = reenter_backup;
  }
  DYAD_C_FUNCTION_END();
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_clear() {
  DYAD_C_FUNCTION_START();
  dyad_rc_t rc = DYAD_RC_OK;
  DYAD_LOG_STDERR("DYAD context is being cleared!\n");
  if (ctx == NULL) {
    rc = DYAD_RC_OK;
    goto clear_region_finish;
  }
  dyad_dtl_finalize(ctx);
  if (ctx->h != NULL) {
    flux_close(ctx->h);
    ctx->h = NULL;
  }
  if (ctx->kvs_namespace != NULL) {
    free(ctx->kvs_namespace);
    ctx->kvs_namespace = NULL;
  }
  if (ctx->prod_managed_path != NULL) {
    free(ctx->prod_managed_path);
    ctx->prod_managed_path = NULL;
  }
  if (ctx->prod_real_path != NULL) {
    free(ctx->prod_real_path);
    ctx->prod_real_path = NULL;
  }
  if (ctx->cons_managed_path != NULL) {
    free(ctx->cons_managed_path);
    ctx->cons_managed_path = NULL;
  }
  if (ctx->cons_real_path != NULL) {
    free(ctx->cons_real_path);
    ctx->cons_real_path = NULL;
  }
  rc = DYAD_RC_OK;
clear_region_finish:;
  DYAD_C_FUNCTION_END();
#ifdef DYAD_PROFILER_DFTRACER
  DFTRACER_C_FINI();
#endif
  return rc;
}

DYAD_DLL_EXPORTED dyad_rc_t dyad_finalize() {
  DYAD_C_FUNCTION_START();
  dyad_rc_t rc = DYAD_RC_OK;
  // DYAD_LOG_STDERR ("DYAD context is being destroyed!\n");
  if (ctx == NULL) {
    rc = DYAD_RC_OK;
    goto finalize_region_finish;
  }
  dyad_clear();
  free(ctx);
  ctx = NULL;
  rc = DYAD_RC_OK;
finalize_region_finish:;
  DYAD_C_FUNCTION_END();
#ifdef DYAD_PROFILER_DFTRACER
  DFTRACER_C_FINI();
#endif
  return rc;
}
