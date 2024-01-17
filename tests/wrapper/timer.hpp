/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef UTILS_TIMER_HPP
#define UTILS_TIMER_HPP

#include <chrono>

inline double get_time ()
{
    using namespace std::chrono;
    return duration_cast<duration<double>> (
               steady_clock::now ().time_since_epoch ())
        .count ();
}

#endif  // UTILS_TIMER_HPP
