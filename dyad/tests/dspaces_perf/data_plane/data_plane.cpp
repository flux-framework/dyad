#include <dspaces.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <array>

#define NS_TO_SECS(ns_var) ((double)ns_var / 1000000000.0)
#define AGG_TIME(time_var, agg_time_var, dtype)                                \
  MPI_Reduce(&time_var, &agg_time_var, 1, dtype, MPI_SUM, 0, MPI_COMM_WORLD)

FILE *redirect_stdout(const char *filename) {
  size_t dir_len = strlen(args.dspaces_timing_dir.c_str());
  bool ends_with_separator =
      (args.dspaces_timing_dir.c_str()[dir_len - 1] == '/');
  size_t filename_len = dir_len + strlen(filename) + 1;
  if (!ends_with_separator) {
    filename_len += 1;
  }
  char *full_filename = (char *)malloc(filename_len * sizeof(char));
  memset(full_filename, 0, filename_len * sizeof(char));
  strcpy(full_filename, args.dspaces_timing_dir.c_str());
  if (!ends_with_separator) {
    strcat(full_filename, "/");
  }
  strcat(full_filename, filename);
  FILE *fp = freopen(full_filename, "a", stdout);
  free(full_filename);
  return fp;
}

int restore_stdout(FILE *freopen_fp) { return fclose(freopen_fp); }

void gen_var_name(char *filename, bool is_local, bool add_rank_if_remote,
                  bool next_local_rank, bool next_node) {
  size_t node_idx = info.rank / args.process_per_node;
  size_t local_rank = info.rank % args.process_per_node;
  if (next_local_rank) {
    local_rank = (local_rank + 1) % args.process_per_node;
  }
  if (next_node) {
    node_idx = (node_idx + 1) % info.num_nodes;
  }
  size_t server_proc_local_idx = info.rank % args.server_ppn;
  size_t global_server_proc_idx =
      node_idx * args.server_ppn + server_proc_local_idx;
  if (is_local) {
    size_t global_rank_from_local =
        node_idx * args.process_per_node + local_rank;
    sprintf(filename, "%s_%zu.bat", args.filename.c_str(),
            global_rank_from_local);
  } else {
    if (add_rank_if_remote) {
      sprintf(filename, "%s_%zu_%zu.bat", args.filename.c_str(),
              global_server_proc_idx, local_rank);
    } else {
      sprintf(filename, "%s_%zu.bat", args.filename.c_str(),
              global_server_proc_idx);
    }
  }
}

int create_files_per_server_process(dspaces_client_t *client, bool is_local,
                                    bool add_rank) {
  int rc = 0;
  char filename[4096];
  size_t file_size = args.request_size * args.iteration;
  gen_var_name(filename, is_local, add_rank, false, false);
  // Clients are connected round-robin to server processes on the same node.
  // To work out which processes should write "files", we first get a node-local
  // rank for each client process by modulo dividing the global rank (info.rank)
  // with the processes per node (args.process_per_node). Then, we floor divide
  // that node-local rank by args.server_ppn. This floor division should only
  // equal 0 for the first "args.server_ppn" ranks to connect to local server
  // processes. Since DataSpaces connects to local processes round-robin, these
  // ranks for which the floor division equals 0 are the "first" processes to
  // connect to each local server processes.
  bool first_rank_per_server_proc =
      (info.rank % args.process_per_node) / args.server_ppn == 0;
  int color = (int)(is_local || add_rank || first_rank_per_server_proc);
  MPI_Comm split_comm;
  int split_rank;
  MPI_Comm_split(MPI_COMM_WORLD, color, info.rank, &split_comm);
  MPI_Comm_rank(split_comm, &split_rank);
  if (is_local || add_rank || first_rank_per_server_proc) {
    std::string rand_data = GenRandom(file_size);
    uint64_t lb = 0;
    uint64_t ub = rand_data.size() - 1;
    uint64_t buf_size = ub + 1;
    dspaces_define_gdim(*client, filename, 1, &buf_size);
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      if (is_local) {
        rc = dspaces_put_local(*client, filename, file_idx, sizeof(char), 1,
                               &lb, &ub, rand_data.data());
      } else {
        rc = dspaces_put(*client, filename, file_idx, sizeof(char), 1, &lb, &ub,
                         rand_data.data());
      }
      if (rc != 0) {
        // TODO log error?
      }
      MPI_Barrier(split_comm);
      if (split_rank == 0) {
        printf("Finished putting file %lu\n", file_idx);
        fflush(stdout);
      }
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
  if (info.rank == 0) {
    printf("Finished putting data into server\n");
    fflush(stdout);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  return rc;
}

void gen_perf_print(size_t data_len, double total_mdata_time,
                    double total_data_time) {
  double agg_mdata_time = 0;
  double agg_data_time = 0;
  AGG_TIME(total_mdata_time, agg_mdata_time, MPI_DOUBLE);
  AGG_TIME(total_data_time, agg_data_time, MPI_DOUBLE);
  if (info.rank == 0) {
    double final_mdata_time = agg_mdata_time / info.comm_size;
    double final_data_time = agg_data_time / info.comm_size;
    printf("[DSPACES_TEST],%10d,%10lu,%10lu,%10.6f,%10.6f,%10.6f,%20.6f\n",
           info.comm_size,                  // Comm Size
           data_len * args.number_of_files, // Total I/O per process
           args.number_of_files,            // Number of mdata ops per process
           final_data_time,                 // Data Time
           final_mdata_time,                // Metadata Time
           data_len * args.number_of_files * info.comm_size / final_data_time /
               1024.0 / 1024.0, // Data Bandwidth
           // TODO change division to be by 1000 instead of 1024
           args.number_of_files * info.comm_size /
               final_mdata_time // Metadata Bandwidth
    );
    fflush(stdout);
  }
}

TEST_CASE("RemoteDataBandwidth",
          "[files= " + std::to_string(args.number_of_files) +
              "]"
              "[file_size= " +
              std::to_string(args.request_size * args.iteration) +
              "]"
              "[parallel_req= " +
              std::to_string(info.comm_size) +
              "]"
              "[num_nodes= " +
              std::to_string(info.comm_size / args.process_per_node) + "]") {
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    REQUIRE(pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE(rc == dspaces_SUCCESS);
    REQUIRE(create_files_per_server_process(&client, false, true) == 0);
    char filename[4096];
    gen_var_name(filename, false, true, false, true);
    size_t data_len = args.request_size * args.iteration;
    char *file_data = NULL;
    int ndim = 1;
    uint64_t lb = 0;
    uint64_t ub = data_len - 1;
    char csv_filename[4096];
    // sprintf(csv_filename, "remote_data_bandwidth_%d.csv", info.rank);
    // FILE* fp = fopen(csv_filename, "w+");
    // FILE* fp = redirect_stdout(csv_filename);
    // REQUIRE(fp != NULL);
    // printf("rank,var_name,version,data_size,mdata_time_ns,data_time_ns\n");
    double total_mdata_time = 0;
    double total_data_time = 0;
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      long long int mdata_time_ns = 0;
      long long int data_time_ns = 0;
      data_time.resumeTime();
      // Using aget instead of get because dyad_get_data also allocates the
      // buffer Also, setting timeout to 0 to prevent blocking for data
      // availability since the data should always be available.
      rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub,
                        (void **)&file_data, -1, &mdata_time_ns, &data_time_ns);
      data_time.pauseTime();
      REQUIRE(rc == dspaces_SUCCESS);
      free(file_data);
      total_mdata_time += NS_TO_SECS(mdata_time_ns);
      total_data_time += NS_TO_SECS(data_time_ns);
    }
    // restore_stdout(fp);
    // AGGREGATE_TIME(data);
    gen_perf_print(data_len, total_mdata_time, total_data_time);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD, 0);
    rc = dspaces_fini(client);
    REQUIRE(rc == dspaces_SUCCESS);
    // fclose(fp);
    REQUIRE(posttest() == 0);
  }
}

TEST_CASE("RemoteDataAggBandwidth",
          "[files= " + std::to_string(args.number_of_files) +
              "]"
              "[file_size= " +
              std::to_string(args.request_size * args.iteration) +
              "]"
              "[parallel_req= " +
              std::to_string(info.comm_size) +
              "]"
              "[num_nodes= " +
              std::to_string(info.comm_size / args.process_per_node) + "]") {
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    REQUIRE(pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE(rc == dspaces_SUCCESS);
    REQUIRE(create_files_per_server_process(&client, false, false) == 0);
    char filename[4096];
    gen_var_name(filename, false, false, false, true);
    char *file_data = NULL;
    size_t data_len = args.request_size * args.iteration;
    int ndim = 1;
    uint64_t lb = 0;
    uint64_t ub = data_len - 1;
    char csv_filename[4096];
    // sprintf(csv_filename, "remote_data_agg_bandwidth_%d.csv", info.rank);
    // FILE* fp = redirect_stdout(csv_filename);
    // FILE* fp = fopen(csv_filename, "w+");
    // REQUIRE(fp != NULL);
    // printf("rank,var_name,version,data_size,mdata_time_ns,data_time_ns\n");
    double total_mdata_time = 0;
    double total_data_time = 0;
    if (info.rank % args.process_per_node != 0)
      usleep(10000);
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      long long int mdata_time_ns = 0;
      long long int data_time_ns = 0;
      data_time.resumeTime();
      // Using aget instead of get because dyad_get_data also allocates the
      // buffer Unlike the previous test, we set the timeout to -1 so it will do
      // any blocking that it might want to do
      // TODO: confirm that the timeout is actually needed to guarantee this
      // type of behavior
      rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub,
                        (void **)&file_data, -1, &mdata_time_ns, &data_time_ns);
      data_time.pauseTime();
      REQUIRE(rc == dspaces_SUCCESS);
      free(file_data);
      total_mdata_time += NS_TO_SECS(mdata_time_ns);
      total_data_time += NS_TO_SECS(data_time_ns);
    }
    // restore_stdout(fp);
    // AGGREGATE_TIME(data);
    // if (info.rank == 0) {
    //   printf("[MANUAL_TIMING] total_data = %10.6f\n", total_data);
    //   fflush(stdout);
    // }
    gen_perf_print(data_len, total_mdata_time, total_data_time);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD, 0);
    rc = dspaces_fini(client);
    REQUIRE(rc == dspaces_SUCCESS);
    // fclose(fp);
    REQUIRE(posttest() == 0);
  }
}

TEST_CASE("LocalProcessDataBandwidth",
          "[files= " + std::to_string(args.number_of_files) +
              "]"
              "[file_size= " +
              std::to_string(args.request_size * args.iteration) +
              "]"
              "[parallel_req= " +
              std::to_string(info.comm_size) +
              "]"
              "[num_nodes= " +
              std::to_string(info.comm_size / args.process_per_node) + "]") {
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    REQUIRE(pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE(rc == dspaces_SUCCESS);
    REQUIRE(create_files_per_server_process(&client, true, true) == 0);
    char filename[4096];
    gen_var_name(filename, true, false, false, false);
    size_t data_len = args.request_size * args.iteration;
    int ndim = 1;
    uint64_t lb = 0;
    uint64_t ub = data_len - 1;
    char *file_data = NULL;
    char csv_filename[4096];
    // sprintf(csv_filename, "local_process_data_bandwidth_%d.csv", info.rank);
    // FILE* fp = redirect_stdout(csv_filename);
    // FILE* fp = fopen(csv_filename, "w+");
    // REQUIRE(fp != NULL);
    // printf("rank,var_name,version,data_size,mdata_time_ns,data_time_ns\n");
    double total_mdata_time = 0;
    double total_data_time = 0;
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      long long int mdata_time_ns = 0;
      long long int data_time_ns = 0;
      data_time.resumeTime();
      // Using aget instead of get because dyad_get_data also allocates the
      // buffer Also, setting timeout to 0 to prevent blocking for data
      // availability since the data should always be available.
      rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub,
                        (void **)&file_data, -1, &mdata_time_ns, &data_time_ns);
      data_time.pauseTime();
      REQUIRE(rc == dspaces_SUCCESS);
      free(file_data);
      total_mdata_time += NS_TO_SECS(mdata_time_ns);
      total_data_time += NS_TO_SECS(data_time_ns);
    }
    // restore_stdout(fp);
    // AGGREGATE_TIME(data);
    gen_perf_print(data_len, total_mdata_time, total_data_time);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD, 0);
    rc = dspaces_fini(client);
    REQUIRE(rc == dspaces_SUCCESS);
    // fclose(fp);
    REQUIRE(posttest() == 0);
  }
}

TEST_CASE("LocalNodeDataBandwidth",
          "[files= " + std::to_string(args.number_of_files) +
              "]"
              "[file_size= " +
              std::to_string(args.request_size * args.iteration) +
              "]"
              "[parallel_req= " +
              std::to_string(info.comm_size) +
              "]"
              "[num_nodes= " +
              std::to_string(info.comm_size / args.process_per_node) + "]") {
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    REQUIRE(pretest() == 0);
    dspaces_client_t client = dspaces_CLIENT_NULL;
    int rc = dspaces_init_mpi(MPI_COMM_WORLD, &client);
    REQUIRE(rc == dspaces_SUCCESS);
    REQUIRE(create_files_per_server_process(&client, true, true) == 0);
    char filename[4096];
    gen_var_name(filename, true, false, true, false);
    size_t data_len = args.request_size * args.iteration;
    int ndim = 1;
    uint64_t lb = 0;
    uint64_t ub = data_len - 1;
    char *file_data = NULL;
    char csv_filename[4096];
    // sprintf(csv_filename, "local_node_data_bandwidth_%d.csv", info.rank);
    // FILE* fp = redirect_stdout(csv_filename);
    // FILE* fp = fopen(csv_filename, "w+");
    // REQUIRE(fp != NULL);
    // printf("rank,var_name,version,data_size,mdata_time_ns,data_time_ns\n");
    double total_mdata_time = 0;
    double total_data_time = 0;
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      long long int mdata_time_ns = 0;
      long long int data_time_ns = 0;
      data_time.resumeTime();
      // Using aget instead of get because dyad_get_data also allocates the
      // buffer Also, setting timeout to 0 to prevent blocking for data
      // availability since the data should always be available.
      rc = dspaces_aget(client, filename, file_idx, ndim, &lb, &ub,
                        (void **)&file_data, -1, &mdata_time_ns, &data_time_ns);
      data_time.pauseTime();
      REQUIRE(rc == dspaces_SUCCESS);
      free(file_data);
      total_mdata_time += NS_TO_SECS(mdata_time_ns);
      total_data_time += NS_TO_SECS(data_time_ns);
    }
    // restore_stdout(fp);
    // AGGREGATE_TIME(data);
    gen_perf_print(data_len, total_mdata_time, total_data_time);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD, 0);
    rc = dspaces_fini(client);
    REQUIRE(rc == dspaces_SUCCESS);
    // fclose(fp);
    REQUIRE(posttest() == 0);
  }
}
