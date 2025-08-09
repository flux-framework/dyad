/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_STREAM_DYAD_STREAM_CORE_HPP
#define DYAD_STREAM_DYAD_STREAM_CORE_HPP

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <climits>
#include <iostream>
#include <string>

#include <dyad/stream/dyad_params.hpp>

extern "C" {
struct dyad_ctx;
}

namespace dyad
{
class dyad_stream_core
{
   public:
    dyad_stream_core ();

    ~dyad_stream_core ();

    void init (const bool reinit = false);
    void init (const dyad_params &p);
    void log_info (const std::string &msg_head) const;

    void finalize ();

    bool is_dyad_producer () const;
    bool is_dyad_consumer () const;

    bool open_sync (const char *path);
    bool close_sync (const char *path);

    void set_initialized ();
    bool chk_initialized () const;

    bool chk_fsync_write () const;
    bool cmp_canonical_path_prefix (bool is_prod, const char *const __restrict__ path);
    std::string get_upath () const;

    int file_lock_exclusive (int fd) const;
    int file_lock_shared (int fd) const;
    int file_unlock (int fd) const;

   private:
    const dyad_ctx *m_ctx;
    dyad_ctx *m_ctx_mutable;
    bool m_initialized;
    bool m_is_prod;
    bool m_is_cons;
    char upath[PATH_MAX];
};

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_STREAM_CORE_HPP
