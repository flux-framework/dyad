/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_PARAMS_HPP
#define DYAD_PARAMS_HPP

#include <string>

namespace dyad {

struct dyad_params {
    bool m_debug;
    // Indicate if the storage associated with the managed path is shared
    // (i.e. visible to all ranks)
    bool m_shared_storage;
    /// The depth of the key hierarchy for path
    unsigned int m_key_depth;
    /// The number of bins used in key hashing
    unsigned int m_key_bins;

    /// The KVS namespace of the sharing context
    std::string m_kvs_namespace;

    /// The path managed by DYAD for consumer
    std::string m_dyad_path_cons;
    /// The path managed by DYAD for producer
    std::string m_dyad_path_prod;

    dyad_params ()
    : m_debug(false),
      m_shared_storage(false),
      m_key_depth(2u), m_key_bins(256u) {}
};

} // end of namespace dyad
#endif // DYAD_PARAMS_HPP
