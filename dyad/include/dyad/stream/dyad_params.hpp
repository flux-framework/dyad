/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_STREAM_DYAD_PARAMS_HPP
#define DYAD_STREAM_DYAD_PARAMS_HPP

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <string>

namespace dyad
{
struct dyad_params {
    bool m_debug;
    /** Indicate if the storage associated with the managed path is shared
     * (i.e. visible to all ranks) */
    bool m_shared_storage;
    /// Indicate if reinitialization is required even if already initinialized
    bool m_reinit;
    /// Enable asynchronous publish by producer
    bool m_async_publish;
    /// Apply fsync after write by producer
    bool m_fsync_write;
    /// The depth of the key hierarchy for path
    unsigned int m_key_depth;
    /// The number of bins used in key hashing
    unsigned int m_key_bins;
    /// The number of brokers sharing node-local storage
    unsigned int m_service_mux;
    /// The DTL to use to move data from producer to consumer
    /// Valid values can be found in dyad_dtl_defs.h from core
    int m_dtl_mode;

    /// The KVS namespace of the sharing context
    std::string m_kvs_namespace;

    /// The path managed by DYAD for consumer
    std::string m_cons_managed_path;
    /// The path managed by DYAD for producer
    std::string m_prod_managed_path;
    /// A relative path is relative to a managed path
    bool m_relative_to_managed_path;

    dyad_params ()
        : m_debug (false),
          m_shared_storage (false),
          m_reinit (false),
          m_async_publish (false),
          m_fsync_write (false),
          m_key_depth (2u),
          m_key_bins (256u),
          m_service_mux (1u),
          m_dtl_mode (0),
          m_kvs_namespace (""),
          m_cons_managed_path (""),
          m_prod_managed_path (""),
          m_relative_to_managed_path (false)
    {
    }
};

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_PARAMS_HPP
