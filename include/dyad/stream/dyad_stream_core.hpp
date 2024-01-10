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

    void init ();
    void init (const dyad_params &p);
    void log_info (const std::string &msg_head) const;

    void finalize ();

    bool is_dyad_producer ();
    bool is_dyad_consumer ();

    bool open_sync (const char *path);
    bool close_sync (const char *path);

    void set_initialized ();
    bool chk_initialized () const;

   private:
    dyad_ctx *m_ctx;
    bool m_initialized;
    bool m_is_prod;
    bool m_is_cons;
};

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_STREAM_CORE_HPP
