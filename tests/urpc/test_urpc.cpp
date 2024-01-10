/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <cstdlib>
#include <iostream>
#include <string>

#include "read_all.h"
#include "urpc_client.h"

int main (int argc, char **argv)
{
    if ((argc != 4) && (argc != 5)) {
        std::cout << "Usage: " << argv[0]
                  << " command_str is_json(0|1) server_rank "
                     "[stdout_result_file]"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::string cmd = argv[1];
    int is_json = atoi (argv[2]);
    uint32_t server_rank = static_cast<uint32_t> (atoi (argv[3]));
    std::string result;

    if (argc == 5) {
        result = argv[4];
    }

    setenv ("URPC_CLIENT_DEBUG", "1", 0);

    std::cout << "Running '" << cmd << "' on the flux broker of rank "
              << server_rank << std::endl;
    std::cout << "Result will be written into " << result << " at rank "
              << urpc_get_my_rank () << std::endl;

    if (is_json) {
        void *inbuf = NULL;
        FILE *fp = NULL;
        fp = fopen (argv[1], "r");
        if (fp == NULL) {
            fprintf (stderr, "Unable to open a file: %s\n", argv[1]);
            exit (-1);
        }
        read_all (fileno (fp), &inbuf);
        fclose (fp);
        cmd = reinterpret_cast<char *> (inbuf);
        std::cout << cmd << std::endl;
    }
    urpc_client (server_rank, cmd.c_str (), result.c_str (), is_json);

    return EXIT_SUCCESS;
}
