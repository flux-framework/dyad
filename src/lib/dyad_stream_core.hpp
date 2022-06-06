/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef DYAD_STREAM_CORE_HPP
#define DYAD_STREAM_CORE_HPP

#include <flux/core.h>
#include <string>
#include "dyad_params.hpp"

namespace dyad {

struct dyad_stream_core {
    flux_t *m_flux;
    bool m_initialized;
    bool m_debug;
    // Indicate if the storage associated with the managed path is shared
    // (i.e. visible to all ranks)
    bool m_shared_storage;
    /// The depth of the key hierarchy for path
    unsigned int m_key_depth;
    /// The number of bins used in key hashing
    unsigned int m_key_bins;
    /// Flux rank
    uint32_t m_rank;

    /// The KVS namespace of the sharing context
    std::string m_kvs_namespace;

    /// The path managed by DYAD for consumer
    std::string m_dyad_path_cons;
    /// The path managed by DYAD for producer
    std::string m_dyad_path_prod;

    dyad_stream_core ()
    : m_flux(nullptr),
      m_initialized(false), m_debug(false),
      m_shared_storage(false),
      m_key_depth(2u), m_key_bins(256u), m_rank(0u) {}

    void init ();
    void init (const dyad_params& p);
    void log_info (const std::string& msg_head) const;

    bool is_dyad_producer ();
    bool is_dyad_consumer ();

    int subscribe_via_flux (const char *user_path);
    int publish_via_flux (const char *user_path);

    bool open_sync (const char *path);
    bool close_sync (const char *path);

  #if DYAD_PERFFLOW
    int dyad_kvs_loopup (const char *topic, uint32_t *owner_rank);
    int dyad_rpc_pack (flux_future_t **f2, uint32_t owner_rank,
                       const char *user_path);
    int dyad_rpc_get_raw (flux_future_t *f2, const char **file_data,
                          int *file_len, const char *user_path);
    int dyad_kvs_commit (flux_kvs_txn_t *txn, flux_future_t **f,
                         const char *user_path);
  #endif // DYAD_PERFFLOW

  #if DYAD_SYNC_DIR
    int sync_directory (const char* path);
  #endif
};

} // end of namespace dyad
#endif // DYAD_STREAM_CORE_HPP