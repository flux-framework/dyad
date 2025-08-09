/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include "io_test.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../../common/utils.h"
#include "io_cpp.hpp"

bool producer (const flist_t& flist, unsigned usec)
{
    int rc = -1;

    for (const auto& f : flist) {
        usleep (static_cast<useconds_t> (usec));
        rc = test_prod_io_stream (f.first, f.second);
        if (rc != 0)
            break;
    }

    if (rc != 0)
        return false;
    return true;
}

bool consumer (const flist_t& flist, unsigned usec, bool verify)
{
    int rc = 0;

    for (const auto& f : flist) {
        rc = test_cons_io_stream (f.first, f.second, verify);
        if (rc != 0) {
            std::cerr << "Consumer error with " << f.first.c_str ()
                      << std::endl;
            break;
        }
        usleep (static_cast<useconds_t> (usec));
    }

    if (rc != 0)
        return false;
    return true;
}

void mkpath (const std::string& path)
{
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    int rc = 0;
    if ((rc = mkdir_as_needed (path.c_str (), m)) < 0) {
        fprintf (stderr, "Directory %s cannot be created. error code (%d)\n",
                 path.c_str (), rc);
        exit (1);
    }
}

std::string get_dyad_path (char* path)
{
    std::string dyad_path = std::string (path);
    if (dyad_path.empty ()) {
        fprintf (stderr, "DYAD_PATH_PRODUCER cannot be defined.\n");
        exit (1);
    }
    if (dyad_path.back () != '/') {
        dyad_path += '/';
    }

    return dyad_path;
}

std::string get_data_path (const std::string& dyad_path,
                           const std::string& context,
                           int iter)
{
    const auto context_iter =
        ((context != "") ? context + '-' + std::to_string (iter) + '/'
                         : std::to_string (iter) + '/');
    return dyad_path + context_iter;
}
