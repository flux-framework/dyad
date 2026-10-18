/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dyad/stream/dyad_stream_api.hpp"

/**
 * @brief Initializes a stream core for one managed directory.
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
    params.m_kvs_namespace = "dyad_stream_reader_lock";
    core.init (params);
}

/**
 * @brief Checks a reader lock using a competing process.
 *
 * @param[in] path            Existing file to test.
 * @param[in] expect_blocked  True when a nonblocking exclusive lock must
 *                            conflict with the reader; false when it must
 *                            succeed.
 * @return @c true when the child observed the expected lock state; otherwise
 *         @c false.
 */
static bool check_exclusive_lock (const std::string &path, bool expect_blocked)
{
    const pid_t pid = fork ();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        const int fd = open (path.c_str (), O_RDWR);
        if (fd < 0) {
            _exit (EXIT_FAILURE);
        }
        struct flock lock = {};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        const int rc = fcntl (fd, F_SETLK, &lock);
        const bool blocked = rc < 0 && (errno == EACCES || errno == EAGAIN);
        if (rc == 0) {
            lock.l_type = F_UNLCK;
            (void)fcntl (fd, F_SETLK, &lock);
        }
        (void)close (fd);
        _exit (blocked == expect_blocked ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    int status = 0;
    return waitpid (pid, &status, 0) == pid && WIFEXITED (status)
           && WEXITSTATUS (status) == EXIT_SUCCESS;
}

/**
 * @brief Publishes the files read by the consumer-side lock test.
 *
 * @param[in] core           Initialized stream core.
 * @param[in] managed_path   Directory where files are published.
 * @return @c true when both files were opened and written; otherwise
 *         @c false.
 */
static bool produce (dyad::dyad_stream_core &core, const std::string &managed_path)
{
    const std::string constructor_path = managed_path + "/reader_lock_constructor.txt";
    dyad::ofstream_dyad constructor_output;
    constructor_output.init (core);
    constructor_output.open (constructor_path);
    if (!constructor_output.is_open ()) {
        return false;
    }
    constructor_output.get_stream () << "constructor reader lock" << std::endl;
    constructor_output.close ();

    const std::string open_path = managed_path + "/reader_lock_open.txt";
    dyad::ofstream_dyad open_output;
    open_output.init (core);
    open_output.open (open_path);
    if (!open_output.is_open ()) {
        return false;
    }
    open_output.get_stream () << "open reader lock" << std::endl;
    open_output.close ();
    return true;
}

/**
 * @brief Verifies reader-lock acquisition and release for both C++ open forms.
 *
 * @param[in] core           Initialized stream core.
 * @param[in] managed_path   Directory containing the published files.
 * @return @c true when constructor and explicit @c open() readers each block
 *         a competing exclusive lock while open and release it on close;
 *         otherwise @c false.
 */
static bool consume_and_check_locks (dyad::dyad_stream_core &core, const std::string &managed_path)
{
    const std::string constructor_path = managed_path + "/reader_lock_constructor.txt";
    if (setenv ("DYAD_PATH_CONSUMER", managed_path.c_str (), 1) != 0) {
        return false;
    }
    dyad::ifstream_dyad constructor_input{constructor_path};
    if (!constructor_input.is_open () || !check_exclusive_lock (constructor_path, true)) {
        return false;
    }
    constructor_input.close ();
    if (!check_exclusive_lock (constructor_path, false)) {
        return false;
    }

    const std::string open_path = managed_path + "/reader_lock_open.txt";
    dyad::ifstream_dyad open_input{core};
    open_input.open (open_path);
    if (!open_input.is_open () || !check_exclusive_lock (open_path, true)) {
        return false;
    }
    open_input.close ();
    return check_exclusive_lock (open_path, false);
}

/**
 * @brief Runs the C++ reader-lock producer or consumer test.
 *
 * @param[in] argc  Number of command-line arguments.
 * @param[in] argv  Managed directory and either @c producer or @c consumer.
 * @return @c 0 on success; @c 1 for invalid arguments or a failed check.
 */
int main (int argc, char **argv)
{
    if (argc != 3) {
        return EXIT_FAILURE;
    }

    dyad::dyad_stream_core core;
    initialize_core (core, argv[1]);
    const std::string role{argv[2]};
    if (role == "producer") {
        return produce (core, argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (role == "consumer") {
        return consume_and_check_locks (core, argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}
