/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "io_test.hpp"

std::vector<double> t_prod;
std::vector<double> t_iter;

void print_times (std::vector<double>& times, const std::string& tag);

int main (int argc, char* argv[])
{
    if ((argc < 4) || (argc > 8)) {
        fprintf (stderr,
                 "Usage: %s dyad_path buffered_IO(0|1) context "
                 "[iter [numfiles size [usec]]]\n",
                 argv[0]);
        return EXIT_FAILURE;
    }

    // Root directory under which shared files are located
    const std::string dyad_path = get_dyad_path (argv[1]);
    mkpath (dyad_path);
    // Whether to use open/close or fopen/fclose
    const bool buffered_io = static_cast<bool> (atoi (argv[2]));
    // The name of the sharing context that uniquely identifies
    // the sharing between producers and consumers. Within this
    // a file name should not be reused.
    const std::string context = argv[3];

    int max_iter = 1;
    size_t num_files = 0u;
    size_t file_size = 0u;
    bool fixed_size_files = false;
    unsigned usec = 0u;

    if (argc > 4) {
        max_iter = atoi (argv[4]);
    }
    if (argc > 6) {
        num_files = static_cast<size_t> (atoi (argv[5]));
        file_size = static_cast<size_t> (atoi (argv[6]));
        fixed_size_files = ((num_files > 0u) && (file_size > 0u));
    }
    if (argc > 7) {
        usec = static_cast<unsigned> (atoi (argv[7]));
        if (usec > 1000000) {
            fprintf (stderr, "usec value must not be larger than 1000000\n");
        }
    }

    t_prod.assign (max_iter, 0);
    t_iter.assign (max_iter, 0);

    std::string data_path;
    std::vector<std::string> files;

    flist_t flist;

    setenv ("DYAD_PATH_PRODUCER", dyad_path.c_str (), 1);
    setenv ("DYAD_KIND_PRODUCER", "1", 1);

    for (int iter = 0; iter < max_iter; iter++) {
        double t1 = get_time ();
        // Prepare inital dataset
        data_path = get_data_path (dyad_path, context, iter);
        mkpath (data_path);
        mkpath (data_path + "rng_state/");
        if (fixed_size_files) {
            set_flist (data_path, flist, num_files, file_size);
        } else {
            set_flist (data_path, flist);
        }

        double t2 = get_time ();
        if (!producer (flist, buffered_io, usec)) {
            return EXIT_FAILURE;
        }
        double t3 = get_time ();

        t_prod[iter] = t3 - t2;
        t_iter[iter] = t3 - t1;
    }

    print_times (t_iter, "Iter :");
    print_times (t_prod, "Prod :");

    return EXIT_SUCCESS;
}

void print_times (std::vector<double>& times, const std::string& tag)
{
    printf ("%s", tag.c_str ());
    for (size_t i = 0ul; i < times.size (); i++) {
        printf ("\t%f", times[i]);
    }
    printf ("\n");
}

/*
 * vi: ts=4 sw=4 expandtab
 */
