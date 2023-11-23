/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <dyad/stream/dyad_stream_core.hpp>

#include <dyad/core/dyad_core.h>
#include <dyad/utils/murmur3.h>
#include <dyad/utils/utils.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <ios>
#endif  // _GNU_SOURCE

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
#include <dyad/common/dyad_flux_log.h>
#include <dyad/core/dyad_envs.h>
#include <fcntl.h>
#include <libgen.h>  // dirname
#include <unistd.h>

namespace dyad
{
/*****************************************************************************
 *                                                                           *
 *                           dyad_stream_core API                             *
 *                                                                           *
 *****************************************************************************/

dyad_stream_core::dyad_stream_core ()
    : m_ctx (NULL), m_initialized (false), m_is_prod (false), m_is_cons (false)
{
}

dyad_stream_core::~dyad_stream_core ()
{
    finalize ();
}

void dyad_stream_core::finalize ()
{
    if (m_ctx != NULL) {
        dyad_finalize (&m_ctx);
        m_ctx = NULL;
        m_initialized = false;
    }
}

void dyad_stream_core::init ()
{
    char *e = NULL;

    if (m_initialized) {
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

    dyad_rc_t rc = dyad_init_env (&m_ctx);

    // TODO figure out if we want to error if init fails
    m_initialized = true;
    log_info ("Stream core is initialized by env variables.");
}

void dyad_stream_core::init (const dyad_params &p)
{
    DPRINTF (m_ctx, "DYAD_WRAPPER: Initializeing DYAD wrapper\n");
    dyad_rc_t rc = dyad_init (p.m_debug,
                              false,
                              p.m_shared_storage,
                              p.m_key_depth,
                              p.m_key_bins,
                              p.m_kvs_namespace.c_str (),
                              p.m_prod_managed_path.c_str (),
                              p.m_cons_managed_path.c_str (),
                              static_cast<dyad_dtl_mode_t> (p.m_dtl_mode),
                              &m_ctx);
    // TODO figure out if we want to error if init fails
    m_initialized = true;
    log_info ("Stream core is initialized by parameters");
}

void dyad_stream_core::log_info (const std::string &msg_head) const
{
    DYAD_LOG_INFO (m_ctx, "=== %s ===\n", msg_head.c_str ());
    DYAD_LOG_INFO (m_ctx, "%s=%s\n", DYAD_PATH_CONSUMER_ENV, m_ctx->cons_managed_path);
    DYAD_LOG_INFO (m_ctx, "%s=%s\n", DYAD_PATH_PRODUCER_ENV, m_ctx->prod_managed_path);
    DYAD_LOG_INFO (m_ctx,
                   "%s=%s\n",
                   DYAD_SYNC_DEBUG_ENV,
                   (m_ctx->debug) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx,
                   "%s=%s\n",
                   DYAD_SHARED_STORAGE_ENV,
                   (m_ctx->shared_storage) ? "true" : "false");
    DYAD_LOG_INFO (m_ctx, "%s=%u\n", DYAD_KEY_DEPTH_ENV, m_ctx->key_depth);
    DYAD_LOG_INFO (m_ctx, "%s=%u\n", DYAD_KEY_BINS_ENV, m_ctx->key_bins);
    DYAD_LOG_INFO (m_ctx, "%s=%s\n", DYAD_KVS_NAMESPACE_ENV, m_ctx->kvs_namespace);
}

bool dyad_stream_core::is_dyad_producer ()
{
    return m_is_prod;
}

bool dyad_stream_core::is_dyad_consumer ()
{
    return m_is_cons;
}

bool dyad_stream_core::open_sync (const char *path)
{
    IPRINTF (m_ctx, "DYAD_SYNC OPEN: enters sync (\"%s\").\n", path);
    if (!m_initialized) {
        // TODO log
        return true;
    }

    if (!is_dyad_consumer ()) {
        return true;
    }

    dyad_rc_t rc = dyad_consume (m_ctx, path);

    if (DYAD_IS_ERROR (rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC OPEN: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF (m_ctx, "DYAD_SYNC OEPN: exists sync (\"%s\").\n", path);
    return true;
}

bool dyad_stream_core::close_sync (const char *path)
{
    IPRINTF (m_ctx, "DYAD_SYNC CLOSE: enters sync (\"%s\").\n", path);
    if (!m_initialized) {
        // TODO log
        return true;
    }

    if (!is_dyad_producer ()) {
        return true;
    }

    dyad_rc_t rc = dyad_produce (m_ctx, path);

    if (DYAD_IS_ERROR (rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC CLOSE: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF (m_ctx, "DYAD_SYNC CLOSE: exists sync (\"%s\").\n", path);
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

}  // end of namespace dyad

/*
 * vi: ts=4 sw=4 expandtab
 */
