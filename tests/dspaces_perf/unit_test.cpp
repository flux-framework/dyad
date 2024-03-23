#include <catch_config.h>
#include <flux/core.h>
#include <mpi.h>
#include <test_utils.h>

/**
 * Test data structures
 */
namespace dyad::test {
struct Info {
    int rank;
    int comm_size;
    int num_nodes;
    size_t num_server_procs;
};
struct Arguments {
    std::string dspaces_timing_dir;
    // MPI Configurations
    size_t process_per_node = 1;
    // DataSpaces Configuration
    size_t server_ppn = 1;
    // Test configuration
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
           cl::Opt(args.process_per_node,
                   "process_per_node")["-p"]["--ppn"]("Processes per node") |
           cl::Opt(args.request_size, "request_size")["-r"]["--request_size"](
               "Transfer size used for performing I/O") |
           cl::Opt(args.iteration,
                   "iteration")["-i"]["--iteration"]("Number of Iterations")  |
           cl::Opt(args.number_of_files,
                   "number_of_files")["-n"]["--number_of_files"]("Number of Files")  |
           cl::Opt(args.server_ppn,
                   "server_ppn")["-s"]["--server_ppn"]("Number of DataSpaces server processes per node") |
           cl::Opt(args.dspaces_timing_dir,
                   "dspaces_timing_dir")["-t"]["--timing_dir"]("Directory to write DataSpaces internal timings") |
           cl::Opt(args.debug,
                   "debug")["-d"]["--debug"]("debug");
}

int pretest() {
    info.num_nodes = info.comm_size / args.process_per_node;
    info.num_server_procs = info.num_nodes * args.server_ppn;
    return 0;
}
int posttest() {
    return 0;
}
#include "data_plane/data_plane.cpp"
// Temporarily disable mdm tests
// #include "mdm/mdm.cpp"
