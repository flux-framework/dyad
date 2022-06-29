/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef IO_CPP_HPP
#define IO_CPP_HPP
#include <string>

int mkdir_of_path (const char* path);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_prod_io_stream (const std::string& pf, const size_t sz);

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
int test_cons_io_stream (const std::string& pf, const size_t sz, const bool verify);


#endif // IO_CPP_HPP
