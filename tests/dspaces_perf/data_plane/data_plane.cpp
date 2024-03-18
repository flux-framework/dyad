#include <fcntl.h>
#include <dspaces.h>

#include <array>

int create_files_per_server_process(dspaces_client_t* client, bool use_local) {
    int rc = 0;
    char filename[4096];
    size_t file_size = args.request_size*args.iteration;
    size_t node_idx = info.rank / args.process_per_node;
    size_t server_proc_local_idx = info.rank % args.server_ppn;
    // Clients are connected round-robin to server processes on the same node.
    // To work out which processes should write "files", we first get a node-local rank for
    // each client process by modulo dividing the global rank (info.rank) with the processes per node
    // (args.process_per_node). Then, we floor divide that node-local rank by args.server_ppn. This
    // floor division should only equal 0 for the first "args.server_ppn" ranks to connect to local
    // server processes. Since DataSpaces connects to local processes round-robin, these ranks for which the
    // floor division equals 0 are the "first" processes to connect to each local server processes.
    bool first_rank_per_server_proc = (info.rank % args.process_per_node) / args.server_ppn == 0;
    if (first_rank_per_server_proc) {
        std::string rand_data = GenRandom(file_size);
        size_t global_server_proc_idx = node_idx * args.server_ppn + server_proc_local_idx;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), global_server_proc_idx);
        uint64_t lb = 0;
        uint64_t ub = rand_data.size()-1;
        uint64_t buf_size = ub + 1;
        dspaces_define_gdim(*client, filename, 1, &buf_size);
        for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
            if (use_local) {
                rc = dspaces_put_local(*client, filename, file_idx, sizeof(char), 1, &lb, &ub, rand_data.data());
            } else {
                rc = dspaces_put(*client, filename, file_idx, sizeof(char), 1, &lb, &ub, rand_data.data());
            }
            if (rc != 0) {
                // TODO log error?
            }
        }
    }
    MPI_Barrier (MPI_COMM_WORLD);
    return rc;
}

TEST_CASE("RemoteDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_server_process(&client, false) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t neighbor_node_idx = ((info.rank / args.process_per_node) + 1) % info.num_nodes;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        uint32_t neighour_server_process_idx = neighbor_node_idx * args.server_ppn + server_proc_local_idx;
        size_t data_len = args.request_size*args.iteration;
        char* file_data = NULL;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), neighbour_server_process_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Also, setting timeout to 0 to prevent blocking for data availability since the data should always be available.
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, 0);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
                info.comm_size, data_len*args.number_of_files,
                total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
        }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}

TEST_CASE("RemoteDataAggBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                    "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                    "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                    "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_broker(&client, false) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t neighbor_node_idx = ((info.rank / args.process_per_node) + 1) % info.num_nodes;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        uint32_t neighour_server_process_idx = neighbor_node_idx * args.server_ppn + server_proc_local_idx;
        size_t data_len = args.request_size*args.iteration;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), neighbour_server_process_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        if (info.rank % args.process_per_node != 0)
            usleep (10000);
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Unlike the previous test, we set the timeout to -1 so it will do any blocking that it might want to do
            // TODO: confirm that the timeout is actually needed to guarantee this type of behavior
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, -1);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
                   info.comm_size, data_len*args.number_of_files,
                   total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
        }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}

TEST_CASE("LocalProcessDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                 "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                 "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                 "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_broker(&client, true) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t data_len = args.request_size*args.iteration;
        size_t node_idx = info.rank / args.process_per_node;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        size_t global_server_proc_idx = node_idx * args.server_ppn + server_proc_local_idx;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), global_server_proc_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        char* file_data =(char*)malloc(data_len);
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Also, setting timeout to 0 to prevent blocking for data availability since the data should always be available.
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, 0);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
                   info.comm_size, data_len*args.number_of_files,
                   total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
        }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}

TEST_CASE("LocalServerProcessDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                       "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                       "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                       "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_broker(&client, false) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t data_len = args.request_size*args.iteration;
        size_t node_idx = info.rank / args.process_per_node;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        size_t global_server_proc_idx = node_idx * args.server_ppn + server_proc_local_idx;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), global_server_proc_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        char* file_data =(char*)malloc(data_len);
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Also, setting timeout to 0 to prevent blocking for data availability since the data should always be available.
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, 0);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
            info.comm_size, data_len*args.number_of_files,
            total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
            }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}

TEST_CASE("LocalClientNodeDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                            "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                            "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                            "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_broker(&client, true) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t data_len = args.request_size*args.iteration;
        size_t node_idx = info.rank / args.process_per_node;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        size_t read_server_proc_local_idx = (server_proc_local_idx + 1) % args.server_ppn;
        size_t global_server_proc_idx = node_idx * args.server_ppn + read_server_proc_local_idx;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), global_server_proc_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        char* file_data =(char*)malloc(data_len);
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Also, setting timeout to 0 to prevent blocking for data availability since the data should always be available.
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, 0);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
                    info.comm_size, data_len*args.number_of_files,
                    total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
        }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}

TEST_CASE("LocalServerNodeDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                                  "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                                  "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                                  "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
    REQUIRE (pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (create_files_per_broker(&client, false) == 0);
    SECTION("Test Max Bandwidth") {
        Timer data_time;
        char filename[4096];
        size_t data_len = args.request_size*args.iteration;
        size_t node_idx = info.rank / args.process_per_node;
        size_t server_proc_local_idx = info.rank % args.server_ppn;
        size_t read_server_proc_local_idx = (server_proc_local_idx + 1) % args.server_ppn;
        size_t global_server_proc_idx = node_idx * args.server_ppn + read_server_proc_local_idx;
        sprintf (filename, "%s_%zu.bat", args.filename.c_str(), global_server_proc_idx);
        int ndim = 0;
        uint64_t buf_size = 0;
        dspaces_get_gdim(client, filename, &ndim, &buf_size);
        uint64_t lb = 0;
        uint64_t ub = buf_size - 1;
        char* file_data =(char*)malloc(data_len);
        for (size_t file_idx=0; file_idx < args.number_of_files; ++file_idx) {
            data_time.resumeTime();
            // Using aget instead of get because dyad_get_data also allocates the buffer
            // Also, setting timeout to 0 to prevent blocking for data availability since the data should always be available.
            rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub, &file_data, 0);
            data_time.pauseTime();
            REQUIRE (rc == dspaces_SUCCESS);
        }
        AGGREGATE_TIME(data);
        if (info.rank == 0) {
            printf("[DSPACES_TEST],%10d,%10lu,%10.6f,%10.6f\n",
                    info.comm_size, data_len*args.number_of_files,
                    total_data/info.comm_size, data_len*args.number_of_files*info.comm_size*info.comm_size/total_data/1024/1024.0);
        }
    }
    rc = dspaces_fini();
    REQUIRE (rc == dspaces_SUCCESS);
    REQUIRE (posttest() == 0);
}
