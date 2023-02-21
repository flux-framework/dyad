/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <mpi.h>

#include <algorithm>  // shuffle
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "../../common/utils.h"
#include "io_test.hpp"

using rng_t = std::minstd_rand;
rng_t rngen;

rng_t& get_rngen ()
{
    return ::rngen;
}

#include <chrono>

std::vector<double> t_prod_io;
std::vector<double> t_cons_io;
std::vector<double> t_prod_sync;
std::vector<double> t_cons_sync;
std::vector<double> t_iter;

int get_partner (int my_rank, int num_ranks, bool randomize = false);
void print_times (std::vector<double>& times, const std::string& tag);
double mpi_sync_io ();

struct Pairing {
    Pairing (int my_rank, int num_ranks, bool randomize = false);
    void set_partner ();
    void set_role (const std::string& dyad_path);

    int m_my_rank;  ///< my rank
    int m_num_ranks;
    int m_partner;       ///< Partner rank
    int m_prod_rank;     ///< Producer rank
    bool m_randomize;    ///< Whether to pair randomly
    bool m_is_producer;  ///< Am I producer
    bool m_is_consumer;  ///< Am I consumer
};

Pairing::Pairing (int my_rank, int num_ranks, bool randomize)
    : m_my_rank (my_rank),
      m_num_ranks (num_ranks),
      m_partner (my_rank),
      m_prod_rank (my_rank),
      m_randomize (randomize),
      m_is_producer (true),
      m_is_consumer (true)
{
    if ((m_my_rank < 0) || (m_num_ranks <= 0) || (m_num_ranks % 2 != 0)) {
        fprintf (stderr, "Invalid rank %d / %d\n", m_my_rank, m_num_ranks);
        exit (EXIT_FAILURE);
    }

    set_partner ();
}

void Pairing::set_partner ()
{
    m_partner = get_partner (m_my_rank, m_num_ranks, m_randomize);

    if (m_randomize) {
        int tmp =
            static_cast<int> (get_rngen () ())  // differentiate iterations
            + (m_my_rank + m_partner) % 3       // differentiate pairs
            + static_cast<int> (
                m_partner > m_my_rank);  // differentiate this and its partner
        m_is_consumer = static_cast<bool> (tmp % 2);
        m_is_producer = ((m_my_rank == m_partner) || !m_is_consumer);
    } else {
        m_is_consumer = (m_partner >= m_my_rank);
        m_is_producer = (m_partner <= m_my_rank);
    }

    m_prod_rank = m_is_consumer ? m_partner : m_my_rank;
}

void Pairing::set_role (const std::string& dyad_path)
{
    if (m_is_consumer) {
        setenv ("DYAD_PATH_CONSUMER", dyad_path.c_str (), 1);
        setenv ("DYAD_KIND_CONSUMER", "1", 1);
    } else {
        setenv ("DYAD_PATH_PRODUCER", dyad_path.c_str (), 1);
        setenv ("DYAD_KIND_PRODUCER", "1", 1);
    }
}

int main (int argc, char* argv[])
{
    int rank = -1;
    int num_ranks = 0;

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &num_ranks);

    if ((argc < 6) || (argc > 11)) {
        fprintf (stderr,
                 "Usage: %s dyad_path context "
                 "verify(0|1) explicit_sync(0|1) randomize(0|seed) "
                 "[iter [numfiles size [usec]]]\n",
                 argv[0]);
        return EXIT_FAILURE;
    }

    // Root directory under which shared files are located
    const std::string dyad_path = get_dyad_path (argv[1]);
    // Whether to use open/close or fopen/fclose
    // The name of the sharing context that uniquely identifies
    // the sharing between producers and consumers. Within this
    // a file name should not be reused.
    const std::string context = argv[2];
    // verify if the bytes read are identical to what have been written
    const bool verify = static_cast<bool> (atoi (argv[3]));
    // Whether to use Dyad interception (implicit) or MPI sync (explicit)
    const bool explicit_sync = static_cast<bool> (atoi (argv[4]));
    const unsigned seed = atoi (argv[5]);
    // Whether to randomize pairing
    const bool randomize = static_cast<bool> (seed);

    int max_iter = 1;
    size_t num_files = 0u;
    size_t file_size = 0u;
    bool fixed_size_files = false;
    unsigned usec = 0u;

    if (argc > 6) {
        max_iter = atoi (argv[6]);
    }
    if (argc > 8) {
        num_files = static_cast<size_t> (atoi (argv[7]));
        file_size = static_cast<size_t> (atoi (argv[8]));
        fixed_size_files = ((num_files > 0u) && (file_size > 0u));
    }
    if (argc > 9) {
        usec = static_cast<unsigned> (atoi (argv[9]));
        if (usec > 1000000) {
            fprintf (stderr, "usec value must not be larger than 1000000\n");
        }
    }

#if 1
    if (rank == 0) {  // in case of using a shared storage
        printf ("dyad_path: %s\n", dyad_path.c_str ());
        printf ("context: %s\n", context.c_str ());
        printf ("verify: %s\n", (verify ? "true" : "false"));
        printf ("explicit_sync: %s\n", (explicit_sync ? "true" : "false"));
        printf ("randomize: %s\n", (randomize ? "true" : "false"));
        printf ("max_iter: %d\n", max_iter);
        printf ("num_files: %lu\n", num_files);
        if (fixed_size_files)
            printf ("file_size: %lu\n", file_size);
        printf ("usec: %u\n", usec);
    }
#endif

    if (rank == 0) {  // in case of using a shared storage
        mkpath (dyad_path);
    }
    MPI_Barrier (MPI_COMM_WORLD);

    if (!explicit_sync) {
        mkpath (dyad_path);
    }

    t_prod_io.assign (max_iter, 0.0);
    t_cons_io.assign (max_iter, 0.0);
    t_prod_sync.assign (max_iter, 0.0);
    t_cons_sync.assign (max_iter, 0.0);
    t_iter.assign (max_iter, 0.0);

    if (!explicit_sync) {
    }

    rngen.seed (seed);
    rngen ();
    std::vector<std::string> files;

    flist_t flist;

    for (int iter = 0; iter < max_iter; iter++) {
        Pairing p (rank, num_ranks, randomize);
        const std::string ctx =
            (context.empty ()
                 ? std::to_string (p.m_prod_rank)
                 : (context + "-" + std::to_string (p.m_prod_rank)));

#if 0
        printf ("Iter %d\t%s %d paired with %d\n", iter,
                (p.m_is_consumer? "Consumer" : "Producer"),
                p.m_my_rank, p.m_partner);
#endif

        double t1 = get_time ();
        // Since every producer writes a same set of files with identical names,
        // there should be exclusive directory assgined to each producer.
        // Here, we create a unique directory name based on the producer rank.
        // This uniqueness represents the context of file sharing. Without the
        // unique directory name, there will be file name conflicts in the KVS
        // and in the shared storage that might be later used to archive the
        // files.
        std::string data_path = get_data_path (dyad_path, ctx, iter);
        if (!explicit_sync) {
            p.set_role (dyad_path);
        }

        if (!explicit_sync || p.m_is_producer) {
            mkpath (data_path);
            mkpath (data_path + "rng_state/");
        }

        if (fixed_size_files) {
            set_flist (data_path, flist, num_files, file_size);
        } else {
            set_flist (data_path, flist);
        }

        if (p.m_is_producer) {  // producer
            double t3 = get_time ();
            if (!producer (flist, usec)) {
                return EXIT_FAILURE;
            }
            t_prod_io[iter] = get_time () - t3;
            if (explicit_sync) {
                t_prod_sync[iter] = mpi_sync_io ();
            }
        } else if (p.m_is_consumer) {  // consumer
            if (explicit_sync) {
                t_cons_sync[iter] = mpi_sync_io ();
            }
            double t3 = get_time ();
            if (!consumer (flist, usec, verify)) {
                return EXIT_FAILURE;
            }
            t_cons_io[iter] = get_time () - t3;
        } else {
            for (const auto& f : flist) {
                fprintf (stderr, "%s\t %lu\n", f.first.c_str (), f.second);
            }
        }

        t_iter[iter] = get_time () - t1;
    }

    print_times (t_iter, std::to_string (rank) + " Iter :");
    print_times (t_prod_io, "Prod io :");
    print_times (t_prod_sync, "Prod sync :");
    print_times (t_cons_io, "Cons io :");
    print_times (t_cons_sync, "Cons sync :");

    MPI_Finalize ();

    return EXIT_SUCCESS;
}

int get_partner (int my_rank, int num_ranks, bool randomize)
{
    int partner = -1;

    if (num_ranks == 0 || my_rank >= num_ranks) {
        return partner;
    }

    // Odd number of ranks
    if ((num_ranks == my_rank + 1) && (num_ranks % 2 != 0)) {
        partner = my_rank;
        return partner;
    }

    std::vector<int> ranks (num_ranks);
    std::iota (ranks.begin (), ranks.end (), 0);

    if (randomize) {
        std::shuffle (ranks.begin (), ranks.end (), get_rngen ());

        for (unsigned int i = 0; i < ranks.size (); i++) {
            if (my_rank == ranks[i]) {
                if (i % 2 == 0) {
                    partner = ranks[i + 1];
                } else {
                    partner = ranks[i - 1];
                }
                break;
            }
        }
    } else {
        partner = (my_rank + ranks.size () / 2) % ranks.size ();
    }

    return partner;
}

void print_times (std::vector<double>& times, const std::string& tag)
{
    printf ("%s", tag.c_str ());
    for (size_t i = 0ul; i < times.size (); i++) {
        printf ("\t%f", times[i]);
    }
    printf ("\n");
}

double mpi_sync_io ()
{
    double t1 = get_time ();
#if 1
    MPI_Barrier (MPI_COMM_WORLD);
#else
    int sendbuf = 1;
    int recvbuf = 0;
    MPI_Allreduce (&sendbuf, &recvbuf, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

    return (get_time () - t1);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
