/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include "dyad_core.h"
#include "dyad_stream_core.hpp"
#include "utils.h"
#include "murmur3.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <ios>
#endif // _GNU_SOURCE

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <ctime>
using namespace std; // std::clock ()
//#include <cstdbool> // c++11

#include <fcntl.h>
#include <libgen.h> // dirname
#include <dlfcn.h>
#include <unistd.h>

namespace dyad {

/*****************************************************************************
 *                                                                           *
 *                           dyad_stream_core API                             *
 *                                                                           *
 *****************************************************************************/

dyad_stream_core::dyad_stream_core(std::ios_base::openmode mode)
    : m_ctx(NULL)
    , m_mode(mode)
    , m_initialized(false)
{}

dyad_stream_core::~dyad_stream_core()
{
    finalize();
}

void dyad_stream_core::finalize()
{
    if (m_ctx != NULL)
    {
        dyad_finalize(m_ctx);
        m_ctx = NULL;
        m_initialized = false;
    }
}

void dyad_stream_core::set_mode(std::ios_base::openmode mode)
{
    m_mode = mode;
}

void dyad_stream_core::init ()
{
    char *e = NULL;

    bool debug = false;
    bool check = false;
    bool shared_storage = false;
    unsigned int key_depth = 0;
    unsigned int key_bins = 0;
    const char *kvs_namespace = NULL;
    const char *cons_managed_path = NULL;
    const char *prod_managed_path = NULL;
    bool intercept = false;

    DPRINTF (m_ctx, "DYAD_WRAPPER: Initializeing DYAD wrapper\n");

    if ((e = getenv ("DYAD_SYNC_DEBUG")) && (atoi (e) != 0)) {
        debug = true;
        enable_debug_dyad_utils ();
    } else {
        debug = false;
        disable_debug_dyad_utils ();
    }

    if ((e = getenv ("DYAD_SHARED_STORAGE")) && (atoi (e) != 0))
        shared_storage = true;
    else
        shared_storage = false;

    if ((e = getenv ("DYAD_KEY_DEPTH")))
        key_depth = atoi (e);
    else
        key_depth = 2;

    if ((e = getenv ("DYAD_KEY_BINS")))
        key_bins = atoi (e);
    else
        key_bins = 256;

    if ((e = getenv ("FLUX_KVS_NAMESPACE")))
    {
        kvs_namespace = e;
    }
    else
    {
        kvs_namespace = NULL;
    }

    if ((e = getenv (DYAD_PATH_CONS_ENV)))
    {
        cons_managed_path = e;
    }
    else
    {
        cons_managed_path = NULL;
    }
    if ((e = getenv (DYAD_PATH_PROD_ENV)))
    {
        prod_managed_path = e;
    }
    else
    {
        prod_managed_path = NULL;
    }

    int rc = dyad_init(debug, check, shared_storage, key_depth,
            key_bins, kvs_namespace, prod_managed_path,
            cons_managed_path, intercept, &m_ctx);

    // TODO figure out if we want to error if init fails
    m_initialized = true;
    log_info ("Stream core is initialized by env variables.");
}

void dyad_stream_core::init (const dyad_params& p)
{
    DPRINTF (m_ctx, "DYAD_WRAPPER: Initializeing DYAD wrapper\n");
    int rc = dyad_init(p.m_debug, false, p.m_shared_storage,
            p.m_key_depth, p.m_key_bins, p.m_kvs_namespace.c_str(),
            p.m_prod_managed_path.c_str(), p.m_cons_managed_path.c_str(),
            false, &m_ctx);
    // TODO figure out if we want to error if init fails
    m_initialized = true;
    log_info ("Stream core is initialized by parameters");
}

void dyad_stream_core::log_info (const std::string& msg_head) const
{
    DYAD_LOG_INFO (m_ctx, "=== %s ===\n", msg_head.c_str ());
    DYAD_LOG_INFO (m_ctx, "%s=%s\n", DYAD_PATH_CONS_ENV, m_ctx->cons_managed_path);
    DYAD_LOG_INFO (m_ctx, "%s=%s\n", DYAD_PATH_PROD_ENV, m_ctx->prod_managed_path);
    DYAD_LOG_INFO (m_ctx, "DYAD_SYNC_DEBUG=%s\n", (m_ctx->debug)? "true": "false");
    DYAD_LOG_INFO (m_ctx, "DYAD_SHARED_STORAGE=%s\n", (m_ctx->shared_storage)? "true": "false");
    DYAD_LOG_INFO (m_ctx, "DYAD_KEY_DEPTH=%u\n", m_ctx->key_depth);
    DYAD_LOG_INFO (m_ctx, "DYAD_KEY_BINS=%u\n", m_ctx->key_bins);
    DYAD_LOG_INFO (m_ctx, "FLUX_KVS_NAMESPACE=%s\n", m_ctx->kvs_namespace);
}

bool dyad_stream_core::is_dyad_producer ()
{
    if (m_mode & std::ios::in)
    {
        return false;
    }
    return true;
}

bool dyad_stream_core::is_dyad_consumer ()
{
    return !is_dyad_producer();
}

bool dyad_stream_core::open_sync (const char *path)
{
    IPRINTF (m_ctx, "DYAD_SYNC OPEN: enters sync (\"%s\").\n", path);
    if (!m_initialized)
    {
        // TODO log
        return true;
    }

    int rc = 0;

    if (! is_dyad_consumer ()) {
        return true;
    }

    rc = dyad_consume(m_ctx, path);

    if (DYAD_IS_ERROR(rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC OPEN: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF (m_ctx, "DYAD_SYNC OEPN: exists sync (\"%s\").\n", path);
    return true;
}

bool dyad_stream_core::close_sync (const char *path)
{
    IPRINTF (m_ctx, "DYAD_SYNC CLOSE: enters sync (\"%s\").\n", path);
    if (!m_initialized)
    {
        // TODO log
        return true;
    }

    int rc = 0;

    if (! is_dyad_producer ()) {
        return true;
    }

    rc = dyad_produce(m_ctx, path);

    if (DYAD_IS_ERROR(rc)) {
        DPRINTF (m_ctx, "DYAD_SYNC CLOSE: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF (m_ctx, "DYAD_SYNC CLOSE: exists sync (\"%s\").\n", path);
    return true;
}

} // end of namespace dyad

/*
 * vi: ts=4 sw=4 expandtab
 * y
 * y
 */
