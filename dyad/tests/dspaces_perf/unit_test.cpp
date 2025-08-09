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
    bool debug_init;
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
    info.debug_init = false;
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
           cl::Opt(args.debug)["-d"]["--debug"]("debug");
}

int pretest() {
    if (!info.debug_init && args.debug) {
        const int HOSTNAME_SIZE = 256;
        char hostname[HOSTNAME_SIZE];
        gethostname(hostname, HOSTNAME_SIZE);
        int pid = getpid();
        char* start_port_str = getenv("VSC_DEBUG_START_PORT");
        int start_port = 10000;
        if (start_port_str != nullptr) {
            start_port = atoi(start_port_str);
        }
        const char* conf_dir = getenv("VSC_DEBUG_CONF_DIR");
        if (conf_dir == nullptr) {
            conf_dir = ".";
        }
        char conf_file[4096];
        sprintf(conf_file, "%s/debug.conf", conf_dir);

        char exe[1024];
        int ret = readlink("/proc/self/exe",exe,sizeof(exe)-1);
        REQUIRE(ret !=-1);
        exe[ret] = 0;
        if (info.rank == 0) {
            remove(conf_file);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_File mpi_fh;
        int status_orig = MPI_File_open(MPI_COMM_WORLD, conf_file, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &mpi_fh);
        REQUIRE(status_orig == MPI_SUCCESS);
        const int buf_len = 16*1024;
        char buffer[buf_len];
        int size;
        if (info.rank == 0) {
            size = sprintf(buffer, "%d\n%s:%d:%s:%d:%d\n", info.comm_size, exe, info.rank, hostname, start_port+info.rank, pid);
        } else {
            size = sprintf(buffer, "%s:%d:%s:%d:%d\n", exe, info.rank, hostname, start_port+info.rank, pid);
        }        
        MPI_Status status;
        MPI_File_write_ordered(mpi_fh, buffer, size, MPI_CHAR, &status);
        int written_bytes;
        MPI_Get_count(&status, MPI_CHAR, &written_bytes);
        REQUIRE(written_bytes == size);
        MPI_File_close(&mpi_fh);
        MPI_Barrier(MPI_COMM_WORLD);
        if (info.rank == 0) {
            printf("%d ready for attach\n", info.comm_size);
            fflush(stdout);
            sleep(60);
        }
        info.debug_init = true;
    }
    info.num_nodes = info.comm_size / args.process_per_node;
    info.num_server_procs = info.num_nodes * args.server_ppn;
    MPI_Barrier(MPI_COMM_WORLD);
    return 0;
}

int posttest() {
    return 0;
}
#include "data_plane/data_plane.cpp"
// Temporarily disable mdm tests
// #include "mdm/mdm.cpp"
