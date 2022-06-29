/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h> // dirname ()
#include "../../common/utils.h" // mkdir_as_needed ()
#include "io_cpp.hpp"
#include "user_cpa.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include "dyad_stream_api.hpp"

#if 0
#include <time.h>
#define PRINT_CLOCK(Who, What)  do { \
    struct timespec ts; \
    clock_gettime (CLOCK_MONOTONIC_RAW, &ts); \
    char buff[100]; \
    strftime (buff, sizeof (buff), "%D %T", gmtime (&ts.tv_sec)); \
    printf (Who " " What " at %s.%09ld UTC\n", buff, ts.tv_nsec); \
} while (0)
#else
#define PRINT_CLOCK(Who, What)
#endif // DYAD_CPA

#if USER_CPA
u_ctx_t my_ctx;
const char *UCPA_OP_STR[UCPA_NUM_OPS+1] =
{
  "Undefined",
  "UCPA_CONS_BEGINS",
  "UCPA_PROD_BEGINS",
  "UCPA_CONS_ENDS",
  "UCPA_PROD_ENDS",
  "UCPA_CONS_OPENS",
  "UCPA_PROD_CLOSES",
  "Noop"
};
#endif // USER_CPA

int mkdir_of_path (const char* path)
{
    char fullpath [PATH_MAX+2] = {'\0'};
    strncpy (fullpath, path, PATH_MAX);
    const char* upper_dir = dirname (fullpath);
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if (mkdir_as_needed (upper_dir, m) < 0) {
        char errmsg [ PATH_MAX+256 ] = {'\0'};
        snprintf (errmsg, PATH_MAX+256, "Directory %s cannot be created.\n",
                  upper_dir);
        perror (errmsg);
        return -1;
    }
    return 0;
}

static void fill_buf (std::string& buffer,
                      size_t buf_sz,
                      const std::string& line)
{
    buffer.clear ();
    if (buf_sz == 0ul) {
        return;
    }
    buffer.reserve (buf_sz);

    unsigned int i = 0u;

    std::string content = std::to_string (i++) + '\t' + line;

    while (buffer.size () + content.size () <= buf_sz) {
        buffer.append (content);
        std::string content = std::to_string (i++) + '\t' + line;
    }
    buffer.append (content, 0ul, buf_sz - buffer.size());
    buffer.back () = '\n';
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static int produce_stream (std::ostream& os, const size_t sz)
{
    size_t n = 0ul;

    std::string buffer;
    const size_t fill_sz = std::min (static_cast<size_t>(PATH_MAX), sz);
    fill_buf (buffer, fill_sz, "test produce_stream");
    if (buffer.empty ()) {
        return -1;
    }

    for (; n + fill_sz <= sz; n += fill_sz) {
        os << buffer;
    }
    if (n < sz) {
        os << buffer.substr (0ul, sz-n);
    }

    return 0;
}

int test_prod_io_stream (const std::string& pf, const size_t sz)
{
    int rc = -1;

    // Create upper directory of the file if it does not exist yet.
    //mkdir_of_path (pf);

    // Open the file to write
    dyad::ofstream_dyad ofs_dyad;
    ofs_dyad.open (pf, std::ofstream::out | std::ios::binary);
    std::ofstream& ofs = ofs_dyad.get_stream ();
    if (!ofs || !ofs.is_open ()) {
        std::cerr << "PROD: Cannot fopen " << pf << " : "
                  << std::string (strerror (errno)) << std::endl;
        return -1;
    }

    // Write some contents into the file as much as `sz` bytes
    rc = produce_stream (ofs, sz);
    if (rc != 0 ) {
        std::cerr << "PROD: Cannot write " << pf << std::endl;
        return -1;
    }

    // Close the file
    ofs_dyad.close ();
    U_REC_TIME(my_ctx, UCPA_PROD_CLOSES);
    PRINT_CLOCK ("Prod", "closes");
    return 0;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static int check_read (const char* buf, size_t sz, const std::string&line)
{
    std::string buffer;
    const size_t fill_sz = std::min (static_cast<size_t>(PATH_MAX), sz);
    fill_buf (buffer, fill_sz, line);

    size_t i = 0ul;
    size_t j = 0ul;
    for (; i < sz ; ++i) {
        if (buffer[j] != buf[i]) {
            fprintf (stderr, "error at %lu th character: %c != %c \n",
                     i, buffer[j], buf[i]);
            return -2;
        }
        if (++j == fill_sz) j = 0ul;
    }

    return 0;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static int consume_stream (std::istream& ifp,
                           const size_t sz,
                           const bool verify)
{
    using char_t = std::istream::char_type;
    std::streampos fsize = 0;

    ifp.seekg(0, std::ios::end);
    fsize = ifp.tellg ();
    ifp.seekg(0, std::ios::beg);

    if (static_cast<size_t>(fsize) != sz * sizeof(char_t)) {
        fprintf (stderr, " read %lu bytes instead of %lu\n",
                 static_cast<size_t>(fsize), sz * sizeof(char_t));
        return -1;
    }
    std::vector<char_t> buf (fsize);
    ifp.read (&buf[0], fsize);

    int rc = ((verify != 0)?
                 check_read (buf.data (), sz, "test produce_stream") : 0);

    return rc;
}

int test_cons_io_stream (const std::string& pf,
                         const size_t sz,
                         const bool verify)
{
    int rc = -1;

    // Create upper directory of the file if it does not exist yet.
    //mkdir_of_path (pf);

    U_REC_TIME (my_ctx, UCPA_CONS_OPENS);
    dyad::ifstream_dyad ifs_dyad;
    ifs_dyad.open (pf, std::ifstream::in | std::ios::binary);
    std::ifstream& ifs = ifs_dyad.get_stream ();
    PRINT_CLOCK ("Cons", "opens");
    //if (!ifs || !ifs.is_open ()) {
    if (!ifs) {
        std::cerr << "CONS: Cannot open " << pf << std::endl;
        return -1;
    }

    // Read the file contents into memory buffer as much as `sz` bytes
    rc = consume_stream (ifs, sz, verify);
    if (rc == -2) {
        std::cerr << "CONS: Wrong bytes read from " << pf << std::endl;
        ifs_dyad.close ();
        return -1;
    } else if (rc == -1) {
        std::cerr << "CONS: Cannot fread " << pf << std::endl;
        ifs_dyad.close ();
        return -1;
    }

    // Close the file
    ifs_dyad.close ();
    return 0;
}
