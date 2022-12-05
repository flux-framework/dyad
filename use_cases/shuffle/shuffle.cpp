#include <fcntl.h>   // open
#include <libgen.h>  // basename dirname
#include <mpi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <vector>

#include "read_all.h"
#include "utils.h"
#include "worker.hpp"

using std::cout;
using std::endl;

int read_list (const std::string& flist_name, std::vector<std::string>& flist)
{
    std::string item_name;
    std::string line;
    std::list<std::string> fnames;

    std::ifstream flist_file;
    flist_file.open (flist_name);
    if (!flist_file) {
        return EXIT_FAILURE;
    }

    while (std::getline (flist_file, line)) {
        fnames.emplace_back (line);
        // std::cout << "file: " << line << std::endl;
    }
    flist.assign (std::make_move_iterator (fnames.begin ()),
                  std::make_move_iterator (fnames.end ()));

    return EXIT_SUCCESS;
}

void create_local_files (const std::string& managed_dir,
                         const Worker& worker,
                         const mode_t md = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
{
    auto range = worker.get_iterator ();
    auto it = range.first;
    auto it_end = range.second;
    const auto rank = worker.get_rank ();
    const auto& flist = worker.get_file_list ();

    std::stringstream ss;
    ss << "Owner rank " << rank << endl;
    std::string header = ss.str ();

    for (; it != it_end; ++it) {
        auto& fn = flist[*it];
        if (fn.empty ()) {
            continue;
        }
        auto path = managed_dir + '/' + fn;
        int fd = open (path.c_str (), O_CREAT | O_WRONLY, md);

        std::stringstream ss2;
        for (unsigned i = 0u; i <= rank; ++i) {
            ss2 << i << endl;
        }
        std::string str = header + fn + '\n' + ss2.str ();

        write (fd, str.c_str (), str.size ());
        close (fd);
    }
}

void read_files (const std::string& managed_dir,
                 const Worker& worker,
                 const bool validate = false)
{
    auto range = worker.get_iterator ();
    auto it = range.first;
    auto it_end = range.second;
    const auto rank = worker.get_rank ();
    const auto& flist = worker.get_file_list ();

    std::stringstream ss;
    ss << rank;

    for (; it != it_end; ++it) {
        auto& fn = flist[*it];
        if (fn.empty ()) {
            continue;
        }
        auto path = managed_dir + '/' + fn;
        int fd = open (path.c_str (), O_RDONLY);
        char* buf;
        auto sz = read_all (fd, reinterpret_cast<void**> (&buf));
        close (fd);

        if (validate) {
            auto fn_copy = managed_dir + "/copy." + ss.str () + '.' + fn;
            std::ofstream ofs;
            ofs.open (fn_copy);
            ofs.write (buf, sz);
            ofs.close ();
        }
    }
}

int main (int argc, char** argv)
{
    int rank;
    int n_ranks;
    unsigned seed = 0u;
    unsigned n_epochs = 3u;
    int rc = 0;
    std::string list_name;
    std::string managed_dir;

    if ((argc != 3) && (argc != 4) && (argc != 5)) {
        cout << "usage: " << argv[0] << " list_file managed_dir [n_epochs [seed]]"
             << endl;
        cout << "   if seed is not given, a random seed is used" << endl;
        return EXIT_SUCCESS;
    }

    list_name = argv[1];
    managed_dir = argv[2];

    if (argc == 4) {
        n_epochs = static_cast<unsigned> (atoi (argv[3]));
    }

    if (argc == 5) {
        seed = static_cast<unsigned> (atoi (argv[4]));
    }

    std::vector<std::string> flist;
    // TODO: root can broadcast
    rc = read_list (list_name, flist);

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &n_ranks);

    if (rc != EXIT_SUCCESS) {
        if (rank == 0) {
            cout << "Failed to read '" << list_name << "'" << endl;
        }
        MPI_Abort (MPI_COMM_WORLD, errno);
        return rc;
    }

    if (rank == 0) {  // if directory is local, everyone should do
        mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
        int c = mkdir_as_needed (managed_dir.c_str (), m);
        if (c < 0) {
            cout << "Could not create directory: '" << managed_dir << "'" << endl;
            MPI_Abort (MPI_COMM_WORLD, errno);
            return EXIT_FAILURE;
        }
    }
    MPI_Barrier (MPI_COMM_WORLD);

    Worker worker;
    worker.set_seed (seed);
    worker.set_rank (rank);
    worker.set_file_list (flist);
    worker.split (n_ranks);

    /*
    const auto rg = worker.get_range();

    std::stringstream ss;

    ss << "Rank " << rank << " [" << rg.first << " - " << rg.second << "]";
    cout << ss.str() << std::endl;
    cout << worker.get_my_list_str() << endl << std::flush;
    */

    if (rank == 0) {
        cout << "Preparing local files under the managed directory" << endl;
    }
    create_local_files (managed_dir, worker, 0644);

    MPI_Barrier (MPI_COMM_WORLD);

    for (unsigned i = 0u; i < n_epochs; ++i) {
        if (rank == 0) {
            cout << "------ Shuffling ... -------" << endl << std::flush;
        }
        MPI_Barrier (MPI_COMM_WORLD);

        worker.shuffle ();
        // cout << i << ' ' << worker.get_my_list_str() << endl << std::flush;
        read_files (managed_dir, worker);
        MPI_Barrier (MPI_COMM_WORLD);
    }

    MPI_Finalize ();

    return rc;
}
