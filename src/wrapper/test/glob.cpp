/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <glob.h> // glob(), globfree()
#include <cstring> // memset()
#include <sstream>
#include <stdexcept>
#include "glob.hpp"

std::vector<std::string> glob(const std::string& pattern) {
    using namespace std;

    // glob struct resides on the stack
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    // do the glob operation
    int rc = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    if (rc != 0) {
        globfree(&glob_result);
        std::string err_msg("glob() failed (rc=");
        err_msg += std::to_string(rc) + ").\n";
        throw std::runtime_error(err_msg);
    }

    // collect all the filenames into a std::list<std::string>
    vector<string> filenames;
    for(size_t i = 0u; i < glob_result.gl_pathc; ++i) {
        filenames.push_back(string(glob_result.gl_pathv[i]));
    }

    // cleanup
    globfree(&glob_result);

    // done
    return filenames;
}
