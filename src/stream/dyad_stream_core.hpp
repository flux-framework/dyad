/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_STREAM_CORE_HPP
#define DYAD_STREAM_CORE_HPP

#include <iostream>
#include <string>
#include "dyad_params.hpp"

extern "C" {
    struct dyad_ctx;
}

namespace dyad {

struct dyad_stream_core {
                                     
    dyad_ctx *m_ctx;                 
                                     
    bool m_initialized;              
                                     
    std::ios_base::openmode m_mode;  
                                     
    dyad_stream_core (std::ios_base::openmode mode=std::ios::in);

    ~dyad_stream_core();

    void set_mode(std::ios_base::openmode mode);

    void init ();
    void init (const dyad_params& p);
    void log_info (const std::string& msg_head) const;

    void finalize();

    bool is_dyad_producer ();
    bool is_dyad_consumer ();

    bool open_sync (const char *path);
    bool close_sync (const char *path);

};

} // end of namespace dyad
#endif // DYAD_STREAM_CORE_HPP
