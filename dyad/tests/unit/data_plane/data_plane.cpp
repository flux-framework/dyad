#include <dyad/common/dyad_logging.h>
#include <dyad/core/dyad_core_int.h>
#include <dyad/core/dyad_ctx.h>
#include <fcntl.h>

#include <cstddef>

int create_files_per_broker() {
  char filename[4096], first_file[4096];
  bool is_first = true;
  size_t file_size = args.request_size * args.iteration;
  size_t node_idx = info.rank / args.process_per_node;
  bool first_rank_per_node = info.rank % args.process_per_node == 0;
  if (first_rank_per_node) {
    fs::create_directories(args.dyad_managed_dir);
    for (size_t broker_idx = 0; broker_idx < args.brokers_per_node;
         ++broker_idx) {
      for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
        size_t global_broker_idx =
            node_idx * args.brokers_per_node + broker_idx;
        if (is_first) {
          sprintf(first_file, "%s/%s_%zu_%zu.bat",
                  args.dyad_managed_dir.c_str(), args.filename.c_str(),
                  global_broker_idx, file_idx);
          std::string cmd = "{ tr -dc '[:alnum:]' < /dev/urandom | head -c " +
                            std::to_string(file_size) + "; } > " + first_file +
                            " ";
          int status = system(cmd.c_str());
          (void)status;
          is_first = false;
        } else {
          sprintf(filename, "%s/%s_%zu_%zu.bat", args.dyad_managed_dir.c_str(),
                  args.filename.c_str(), global_broker_idx, file_idx);
          std::string cmd = "cp " + std::string(first_file) + " " + filename;
          int status = system(cmd.c_str());
          (void)status;
        }
      }
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
  return 0;
}
// clang-format off
TEST_CASE("RemoteDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]" 
                                "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(create_files_per_broker() == 0);
  dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  auto ctx = dyad_ctx_get();
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    char filename[4096];
    uint32_t neighour_broker_idx = (info.broker_idx + 1) % info.broker_size;
    dyad_metadata_t mdata;
    mdata.owner_rank = neighour_broker_idx;
    size_t data_len = args.request_size * args.iteration;
    char *file_data = NULL;
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(filename, "%s_%u_%zu.bat", args.filename.c_str(),
              neighour_broker_idx, file_idx);
      mdata.fpath = filename;
      data_time.resumeTime();
      auto rc = dyad_get_data(ctx, &mdata, &file_data, &data_len);
      data_time.pauseTime();
      REQUIRE(rc >= 0);
    }
    AGGREGATE_TIME(data);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             data_len * args.number_of_files, total_data / info.comm_size,
             data_len * args.number_of_files * info.comm_size * info.comm_size /
                 total_data / 1024 / 1024.0);
    }
  }
  auto rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(posttest() == 0);
}
// clang-format off
TEST_CASE("RemoteDataAggBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                    "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                    "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                    "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(create_files_per_broker() == 0);
  dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  auto ctx = dyad_ctx_get();
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    char filename[4096], upath[4096];
    uint32_t neighour_broker_idx = (info.broker_idx + 1) % info.broker_size;
    dyad_metadata_t mdata;
    mdata.owner_rank = neighour_broker_idx;
    size_t data_len = args.request_size * args.iteration;
    if (info.rank % args.process_per_node != 0)
      usleep(10000);
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(upath, "%s_%u_%zu.bat", args.filename.c_str(),
              neighour_broker_idx, file_idx);
      sprintf(filename, "%s/%s_%u_%zu.bat", args.dyad_managed_dir.c_str(),
              args.filename.c_str(), neighour_broker_idx, file_idx);
      mdata.fpath = upath;
      data_time.resumeTime();
      auto rc = dyad_consume_w_metadata(ctx, filename, &mdata);
      data_time.pauseTime();
      REQUIRE(rc >= 0);
    }
    AGGREGATE_TIME(data);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             data_len * args.number_of_files, total_data / info.comm_size,
             data_len * args.number_of_files * info.comm_size * info.comm_size /
                 total_data / 1024 / 1024.0);
    }
  }
  auto rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(posttest() == 0);
}

// clang-format off
TEST_CASE("LocalProcessDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                 "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                 "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                 "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(create_files_per_broker() == 0);
  dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    char filename[4096];
    size_t data_len = args.request_size * args.iteration;
    char *file_data = (char *)malloc(data_len);
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(filename, "%s/%s_%u_%zu.bat", args.dyad_managed_dir.c_str(),
              args.filename.c_str(), info.broker_idx, file_idx);
      data_time.resumeTime();
      int fd = open(filename, O_RDONLY);
      data_time.pauseTime();
      REQUIRE(fd != -1);
      data_time.resumeTime();
      int bytes = read(fd, file_data, data_len);
      data_time.pauseTime();
      REQUIRE((size_t)bytes == data_len);
      data_time.resumeTime();
      int status = close(fd);
      data_time.pauseTime();
      REQUIRE(status == 0);
    }
    AGGREGATE_TIME(data);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             data_len * args.number_of_files, total_data / info.comm_size,
             data_len * args.number_of_files * info.comm_size * info.comm_size /
                 total_data / 1024 / 1024.0);
    }
  }
  auto rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(posttest() == 0);
}
// clang-format off
TEST_CASE("LocalNodeDataBandwidth", "[files= " + std::to_string(args.number_of_files) +"]"
                                       "[file_size= " + std::to_string(args.request_size*args.iteration) +"]"
                                       "[parallel_req= " + std::to_string(info.comm_size) +"]"
                                       "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(create_files_per_broker() == 0);
  dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  SECTION("Test Max Bandwidth") {
    Timer data_time;
    char filename[4096];
    size_t data_len = args.request_size * args.iteration;
    char *file_data = (char *)malloc(data_len);
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(filename, "%s/%s_%u_%zu.bat", args.dyad_managed_dir.c_str(),
              args.filename.c_str(), info.broker_idx, file_idx);
      data_time.resumeTime();
      int fd = open(filename, O_RDONLY);
      data_time.pauseTime();
      REQUIRE(fd != -1);
      data_time.resumeTime();
      int bytes = read(fd, file_data, data_len);
      data_time.pauseTime();
      REQUIRE((size_t)bytes == data_len);
      data_time.resumeTime();
      int status = close(fd);
      data_time.pauseTime();
      REQUIRE(status == 0);
    }
    AGGREGATE_TIME(data);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             data_len * args.number_of_files, total_data / info.comm_size,
             data_len * args.number_of_files * info.comm_size * info.comm_size /
                 total_data / 1024 / 1024.0);
    }
  }
  auto rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(posttest() == 0);
}