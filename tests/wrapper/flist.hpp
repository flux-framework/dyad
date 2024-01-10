/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef FLIST_SET
#define FLIST_SET
#include <string>
#include <vector>

using flist_t = std::vector<std::pair<std::string, size_t> >;
void set_flist (std::string base, flist_t& flist);
void set_flist (std::string base,
                flist_t& flist,
                const size_t num_files,
                const size_t fixed_size);

#endif
