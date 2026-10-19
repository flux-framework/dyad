/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <fstream>
#include <string>
#include <utility>

#include "dyad/stream/dyad_stream_api.hpp"

/**
 * @brief Initializes a stream core for a single managed directory.
 *
 * @param[out] core          Stream core to initialize.
 * @param[in] managed_path   Producer and consumer managed directory.
 * @return Nothing.
 */
static void initialize_core (dyad::dyad_stream_core &core, const std::string &managed_path)
{
    dyad::dyad_params params;
    params.m_cons_managed_path = managed_path;
    params.m_prod_managed_path = managed_path;
    params.m_kvs_namespace = "dyad_stream_move";
    core.init (params);
}

/**
 * @brief Writes and publishes files after stream move construction and assignment.
 *
 * @param[in] core            Initialized stream core.
 * @param[in] managed_path    Directory containing the test files.
 * @return Nothing.
 */
static void produce (dyad::dyad_stream_core &core, const std::string &managed_path)
{
    const std::string constructor_path = managed_path + "/move_constructor.txt";
    dyad::ofstream_dyad constructor_source;
    constructor_source.init (core);
    constructor_source.open (constructor_path);
    constructor_source.get_stream () << "move constructor" << std::endl;
    dyad::ofstream_dyad constructor_destination{std::move (constructor_source)};
    constructor_destination.close ();

    const std::string assignment_path = managed_path + "/move_assignment.txt";
    dyad::ofstream_dyad assignment_source;
    assignment_source.init (core);
    assignment_source.open (assignment_path);
    assignment_source.get_stream () << "move assignment" << std::endl;
    dyad::ofstream_dyad assignment_destination;
    assignment_destination = std::move (assignment_source);
    assignment_destination.close ();
}

/**
 * @brief Consumes and verifies the files published by @c produce().
 *
 * @param[in] core            Initialized stream core.
 * @param[in] managed_path    Directory containing the test files.
 * @return @c true when both files contain their expected text; otherwise
 *         @c false.
 */
static bool consume (dyad::dyad_stream_core &core, const std::string &managed_path)
{
    dyad::ifstream_dyad constructor_input;
    constructor_input.init (core);
    constructor_input.open (managed_path + "/move_constructor.txt");
    std::string constructor_text;
    std::getline (constructor_input.get_stream (), constructor_text);
    constructor_input.close ();

    dyad::ifstream_dyad assignment_input;
    assignment_input.init (core);
    assignment_input.open (managed_path + "/move_assignment.txt");
    std::string assignment_text;
    std::getline (assignment_input.get_stream (), assignment_text);
    assignment_input.close ();

    return constructor_text == "move constructor" && assignment_text == "move assignment";
}

/**
 * @brief Runs the C++ stream move-semantics producer or consumer test.
 *
 * @param[in] argc  Number of command-line arguments.
 * @param[in] argv  Arguments: managed directory and @c producer or @c consumer.
 * @return @c 0 on success; @c 1 for invalid arguments or a failed verification.
 */
int main (int argc, char **argv)
{
    if (argc != 3) {
        return 1;
    }

    dyad::dyad_stream_core core;
    initialize_core (core, argv[1]);

    const std::string role{argv[2]};
    if (role == "producer") {
        produce (core, argv[1]);
        return 0;
    }
    if (role == "consumer") {
        return consume (core, argv[1]) ? 0 : 1;
    }
    return 1;
}
