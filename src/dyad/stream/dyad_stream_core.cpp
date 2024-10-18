/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <ios>
#endif  // _GNU_SOURCE

#include <fcntl.h>
#include <libgen.h>  // dirname
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

using namespace std;  // std::clock ()
// #include <cstdbool> // c++11

#include <dlfcn.h>

#include <dyad/stream/dyad_stream_core.hpp>

#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/core/dyad_core.h>
#include <dyad/core/dyad_ctx.h>
#include <dyad/utils/murmur3.h>
#include <dyad/utils/utils.h>

namespace dyad
{
/*****************************************************************************
 *                                                                           *
 *                           dyad_stream_core API                            *
 *                                                                           *
 *****************************************************************************/

dyad_stream_core::dyad_stream_core ()
    : m_ctx (NULL),
      m_ctx_mutable (NULL),
      m_initialized (false),
      m_is_prod (false),
      m_is_cons (false)
{
    DYAD_CPP_FUNCTION ();
}

dyad_stream_core::~dyad_stream_core ()
{
    finalize ();
}

void dyad_stream_core::finalize ()
{
    DYAD_CPP_FUNCTION ();
    if (m_ctx != NULL) {
        // dyad_finalize (&m_ctx);
        m_ctx = m_ctx_mutable = NULL;
        m_initialized = false;
    }
}

void dyad_stream_core::init (const bool reinit)
{
    DYAD_CPP_FUNCTION ();
    bool reinit_env = false;
    char *e = NULL;

    if ((e = getenv (DYAD_REINIT_ENV))) {
        reinit_env = true;
    } else {
        reinit_env = false;
    }

    if (!(reinit || reinit_env) && m_initialized) {
        return;
    }

    if ((e = getenv (DYAD_PATH_CONSUMER_ENV))) {
        m_is_cons = (strlen (e) != 0);
    } else {
        m_is_cons = false;
    }
    if ((e = getenv (DYAD_PATH_PRODUCER_ENV))) {
        m_is_prod = (strlen (e) != 0);
    } else {
        m_is_prod = false;
    }

    if (reinit || reinit_env || !(m_ctx = m_ctx_mutable = dyad_ctx_get ())) {
        dyad_ctx_init (DYAD_COMM_RECV, NULL);
        m_ctx = m_ctx_mutable = dyad_ctx_get ();
        log_info ("Stream core is initialized by env variables.");
    } else {
        log_info ("Stream core skips initialization as it has already been initialized.");
    }

    // TODO figure out if we want to error if init fails
    m_initialized = true;
}

void dyad_stream_core::init (const dyad_params &p)
{
    DYAD_CPP_FUNCTION ();
    DYAD_LOG_DEBUG (m_ctx, "DYAD_WRAPPER: Initializeing DYAD wrapper");
    dyad_rc_t rc = dyad_init (p.m_debug,
                              false,
                              p.m_shared_storage,
                              p.m_reinit,
                              p.m_async_publish,
                              p.m_fsync_write,
                              p.m_key_depth,
                              p.m_key_bins,
                              p.m_service_mux,
                              p.m_kvs_namespace.c_str (),
                              p.m_prod_managed_path.c_str (),
                              p.m_cons_managed_path.c_str (),
                              p.m_relative_to_managed_path,
                              dyad_dtl_mode_name[static_cast<dyad_dtl_mode_t> (p.m_dtl_mode)],
                              DYAD_COMM_RECV,
                              NULL);
#if defined(DYAD_HAS_STD_FSTREAM_FD)
    m_ctx_mutable->use_fs_locks = true;
#else
    // Rely on the KVS-based synchronization and disable checking for fs lock based logic.
    m_ctx_mutable->use_fs_locks = false;
#endif
    (void)rc;
    // TODO figure out if we want to error if init fails
    m_initialized = true;
    log_info ("Stream core is initialized by parameters");
}

void dyad_stream_core::log_info (const std::string &msg_head) const
{
    DYAD_CPP_FUNCTION ();
    DYAD_LOG_INFO (m_ctx, "=== %s ===", msg_head.c_str ());
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_PATH_CONSUMER_ENV, m_ctx->cons_managed_path);
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_PATH_PRODUCER_ENV, m_ctx->prod_managed_path);
    DYAD_LOG_INFO (m_ctx,
                   "%s=%s",
                   DYAD_PATH_RELATIVE_ENV,
                   (m_ctx->relative_to_managed_path) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_SYNC_DEBUG_ENV, (m_ctx->debug) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx,
                   "%s=%s",
                   DYAD_SHARED_STORAGE_ENV,
                   (m_ctx->shared_storage) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx,
                   "%s=%s",
                   DYAD_ASYNC_PUBLISH_ENV,
                   (m_ctx->async_publish) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_FSYNC_WRITE_ENV, (m_ctx->fsync_write) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx, "%s=%u", DYAD_KEY_DEPTH_ENV, m_ctx->key_depth);
    DYAD_LOG_INFO (m_ctx, "%s=%u", DYAD_KEY_BINS_ENV, m_ctx->key_bins);
    DYAD_LOG_INFO (m_ctx, "%s=%u", DYAD_SERVICE_MUX_ENV, m_ctx->service_mux);
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_KVS_NAMESPACE_ENV, m_ctx->kvs_namespace);
    DYAD_LOG_INFO (m_ctx, "%s=%s", DYAD_DTL_MODE_ENV, getenv (DYAD_DTL_MODE_ENV));
}

bool dyad_stream_core::is_dyad_producer () const
{
    return m_is_prod;
}

bool dyad_stream_core::is_dyad_consumer () const
{
    return m_is_cons;
}

bool dyad_stream_core::open_sync (const char *path)
{
    DYAD_CPP_FUNCTION ();
    DYAD_CPP_FUNCTION_UPDATE ("path", path);
    DYAD_LOG_DEBUG (m_ctx, "DYAD_SYNC OPEN: enters sync (\"%s\").", path);
    if (!m_initialized) {
        // TODO log
        return true;
    }

    if (!is_dyad_consumer ()) {
        return true;
    }

    dyad_rc_t rc = dyad_consume (m_ctx_mutable, path);

    if (DYAD_IS_ERROR (rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC OPEN: failed sync (\"%s\").", path);
        return false;
    }

    DYAD_LOG_DEBUG (m_ctx, "DYAD_SYNC OEPN: exists sync (\"%s\").\n", path);
    return true;
}

bool dyad_stream_core::close_sync (const char *path)
{
    DYAD_CPP_FUNCTION ();
    DYAD_CPP_FUNCTION_UPDATE ("path", path);
    DYAD_LOG_DEBUG (m_ctx, "DYAD_SYNC CLOSE: enters sync (\"%s\").\n", path);
    if (!m_initialized) {
        // TODO log
        return true;
    }

    if (!is_dyad_producer ()) {
        return true;
    }

    dyad_rc_t rc = dyad_produce (m_ctx_mutable, path);

    if (DYAD_IS_ERROR (rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC CLOSE: failed sync (\"%s\").\n", path);
        return false;
    }

    DYAD_LOG_DEBUG (m_ctx, "DYAD_SYNC CLOSE: exists sync (\"%s\").\n", path);
    return true;
}

void dyad_stream_core::set_initialized ()
{
    m_initialized = true;
}

bool dyad_stream_core::chk_initialized () const
{
    return m_initialized;
}

bool dyad_stream_core::chk_fsync_write () const
{
    return m_ctx->fsync_write;
}

int dyad_stream_core::file_lock_exclusive (int fd) const
{
    struct flock exclusive_flock;

    dyad_rc_t rc = dyad_excl_flock (m_ctx, fd, &exclusive_flock);

    if (DYAD_IS_ERROR (rc)) {
        dyad_release_flock (m_ctx, fd, &exclusive_flock);
    }

    return rc;
}

int dyad_stream_core::file_lock_shared (int fd) const
{
    struct flock shared_flock;

    dyad_rc_t rc = dyad_excl_flock (m_ctx, fd, &shared_flock);

    if (DYAD_IS_ERROR (rc)) {
        dyad_release_flock (m_ctx, fd, &shared_flock);
    }

    return rc;
}

int dyad_stream_core::file_unlock (int fd) const
{
    struct flock a_flock;

    return dyad_release_flock (m_ctx, fd, &a_flock);
}

bool dyad_stream_core::cmp_canonical_path_prefix (bool is_prod, const char *const __restrict__ path)
{
    memset (upath, '\0', PATH_MAX);
    return ::cmp_canonical_path_prefix (m_ctx, is_prod, path, upath, PATH_MAX);
}

std::string dyad_stream_core::get_upath () const
{
    return std::string{upath};
}

}  // end of namespace dyad

/*
 * vi: ts=4 sw=4 expandtab
 */
