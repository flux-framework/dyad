/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <iostream>
//#include <filesystem>
//#include <experimental/filesystem>
#include "dyad_stream_api.hpp"

void producer_open (dyad::dyad_stream_core& dyad, const std::string& filename)
{
    dyad::ofstream_dyad ofs_dyad;
    ofs_dyad.init (dyad);
    ofs_dyad.open (filename);
    std::ofstream& ofs = ofs_dyad.get_stream ();
    ofs << "test stream with explicit open() and close()." << std::endl;
    ofs_dyad.close();
}

void consumer_open (dyad::dyad_stream_core& dyad, const std::string& filename)
{
    dyad::ifstream_dyad ifs_dyad;
    ifs_dyad.init (dyad);
    ifs_dyad.open (filename);
    std::ifstream& ifs = ifs_dyad.get_stream ();
    std::string line;
    ifs >> line;
    std::cout << line << std::endl;
    ifs_dyad.close ();
}

void producer (dyad::dyad_stream_core& dyad, const std::string& filename)
{
    dyad::ofstream_dyad ofs_dyad (filename);
    ofs_dyad.init (dyad);
    std::ofstream& ofs = ofs_dyad.get_stream ();
    ofs << "test stearm with constructor and desctructor." << std::endl;
    ofs_dyad.close ();
}

void consumer (dyad::dyad_stream_core& dyad, const std::string& filename)
{
    dyad::ifstream_dyad ifs_dyad (filename);
    ifs_dyad.init (dyad);
    std::ifstream& ifs = ifs_dyad.get_stream ();
    std::string line;
    ifs >> line;
    std::cout << line << std::endl;
    ifs_dyad.close ();
}

int main (int argc, char** argv)
{
    if (argc != 5) {
        std::cout << "Usage: " << argv[0]
                  << " dyad_path file read(0)/write(1) use_open_close"
                  << std::endl;
        return 0;
    }

    std::string dyad_path = argv[1];
    std::string filename = argv[2];
    bool writemode = static_cast<bool>(atoi(argv[3]));
    bool use_open_close = static_cast<bool>(atoi(argv[4]));

    dyad::dyad_params dparams;
    dparams.m_dyad_path_cons = dyad_path;
    dparams.m_dyad_path_prod = dyad_path;
    dparams.m_debug = true;
    dparams.m_shared_storage = false;
    dparams.m_kvs_namespace = "test";
    dparams.m_key_depth = 2;
    dparams.m_key_bins = 256;

    dyad::dyad_stream_core dyad;
    dyad.init (dparams);

    if (use_open_close) {
        if (writemode) {
            producer_open (dyad, filename);
        } else {
            consumer_open (dyad, filename);
        }
    } else {
        if (writemode) {
            producer (dyad, filename);
        } else {
            consumer( dyad, filename);
        }
    }

    return 0;
}
