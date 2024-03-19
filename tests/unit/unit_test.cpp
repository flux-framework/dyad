#include <catch_config.h>
#include <flux/core.h>
#include <mpi.h>
#include <test_utils.h>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

/**
 * Test data structures
 */
namespace dyad::test {
struct Info {
    int rank;
    int comm_size;
    int num_nodes;
    int num_brokers;
    flux_t* flux_handle;
    uint32_t broker_idx;
    uint32_t broker_size;
};
struct Arguments {
    // MPI Configurations
    size_t process_per_node = 1;
    size_t brokers_per_node = 1;
    // DYAD Configuration
    fs::path dyad_managed_dir = "~/dyad/dmd";
    // Test configuration
    fs::path pfs = "~/dyad/pfs";
    std::string filename = "test.dat";
    size_t number_of_files = 1;
    size_t request_size = 65536;
    size_t iteration = 8;
    bool debug = false;
};
}  // namespace dyad::test

dyad::test::Arguments args;
dyad::test::Info info;
/**
 * Overridden methods for catch
 */

int init(int* argc, char*** argv) {
    //  fprintf(stdout, "Initializing MPI\n");
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &info.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &info.comm_size);
    info.flux_handle = flux_open (NULL, 0);
    if (args.debug && info.rank == 0) {
        printf("%d ready for attach\n", info.comm_size);
        fflush(stdout);
        getchar();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    return 0;
}
int finalize() {
    MPI_Finalize();
    return 0;
}
cl::Parser define_options() {
    return cl::Opt(args.filename, "filename")["-f"]["--filename"](
        "Filename to be use for I/O.") |
           cl::Opt(args.pfs, "pfs")["-d"]["--pfs"](
               "Directory used for performing I/O (default pfs)") |
           cl::Opt(args.dyad_managed_dir, "dmd")["-d"]["--dmd"](
               "Directory used for DYAD Managed Directory") |
           cl::Opt(args.process_per_node,
                   "process_per_node")["-p"]["--ppn"]("Processes per node") |
           cl::Opt(args.request_size, "request_size")["-r"]["--request_size"](
               "Transfer size used for performing I/O") |
           cl::Opt(args.iteration,
                   "iteration")["-i"]["--iteration"]("Number of Iterations")  |
           cl::Opt(args.number_of_files,
                   "number_of_files")["-n"]["--number_of_files"]("Number of Files")  |
           cl::Opt(args.brokers_per_node,
                   "brokers_per_node")["-b"]["--brokers_per_node"]("Number of Brokers per node") |
           cl::Opt(args.debug,
                   "debug")["-d"]["--debug"]("debug");
}

int pretest() {
    info.num_nodes = info.comm_size / args.process_per_node;
    info.num_brokers = info.num_nodes * args.brokers_per_node;
    flux_get_rank (info.flux_handle, &info.broker_idx);
    flux_get_size (info.flux_handle, &info.broker_size);
    return 0;
}
int posttest() {
    return 0;
}
int clean_directories() {
    auto file_pt = args.pfs.string() + args.filename;
    std::string cmd = "rm -rf " + file_pt + "*";
    int status = system (cmd.c_str ());
    (void) status;
    file_pt = args.dyad_managed_dir.string() + args.filename;
    cmd = "rm -rf " + file_pt + "*";
    status = system (cmd.c_str ());
    (void) status;
    return 0;
}
#include "data_plane/data_plane.cpp"
#include "mdm/mdm.cpp"
