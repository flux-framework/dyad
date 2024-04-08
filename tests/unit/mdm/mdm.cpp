
#include <dyad/utils/utils.h>
#include <fcntl.h>

#include <climits>
#include <string>

// clang-format off
TEST_CASE("LocalFSLookup",  "[number_of_lookups= " + std::to_string(args.number_of_files) +"]"
                            "[parallel_req= " + std::to_string(info.comm_size) +"]"
                            "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  REQUIRE(clean_directories() == 0);
  dyad_rc_t rc = dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  REQUIRE(rc >= 0);
  auto ctx = dyad_ctx_get();
  struct flock exclusive_lock;
  SECTION("Throughput") {
    char filename[4096];
    Timer kvs_time;
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(filename, "%s/%s_%u_%zu.bat", args.dyad_managed_dir.c_str(),
              args.filename.c_str(), info.broker_idx, file_idx);
      INFO("The file " << filename << " rank " << info.rank);
      kvs_time.resumeTime();
      int lock_fd = open(filename, O_RDWR | O_CREAT, 0666);
      kvs_time.pauseTime();
      REQUIRE(lock_fd != -1);

      kvs_time.resumeTime();
      rc = dyad_excl_flock(ctx, lock_fd, &exclusive_lock);
      kvs_time.pauseTime();
      REQUIRE(rc >= 0);

      kvs_time.resumeTime();
      auto file_size = get_file_size(lock_fd);
      kvs_time.pauseTime();
      (void)file_size;
      kvs_time.resumeTime();
      dyad_release_flock(ctx, lock_fd, &exclusive_lock);
      int status = close(lock_fd);
      kvs_time.pauseTime();
      REQUIRE(status == 0);
    }
    AGGREGATE_TIME(kvs);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             args.number_of_files, total_kvs / info.comm_size,
             args.number_of_files * info.comm_size * info.comm_size /
                 total_kvs / 1000 / 1000);
    }
  }
  rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(clean_directories() == 0);
  REQUIRE(posttest() == 0);
}
// clang-format off
TEST_CASE("LocalKVSLookup",  "[number_of_lookups= " + std::to_string(args.number_of_files) +"]"
                             "[parallel_req= " + std::to_string(info.comm_size) +"]"
                             "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  dyad_rc_t rc = dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  REQUIRE(rc >= 0);
  auto ctx = dyad_ctx_get();
  SECTION("Throughput") {
    Timer kvs_time;
    char my_filename[4096], lookup_filename[4096];
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(my_filename, "%s/%s_%u_%d_%zu.bat", args.dyad_managed_dir.c_str(),
              args.filename.c_str(), info.broker_idx, info.rank, file_idx);
      sprintf(lookup_filename, "%s_%u_%d_%zu.bat", args.filename.c_str(),
              info.broker_idx, info.rank, file_idx);
      rc = dyad_commit(ctx, my_filename);
      REQUIRE(rc >= 0);
      dyad_metadata_t* mdata;
      const size_t topic_len = PATH_MAX;
      char topic[PATH_MAX + 1] = {'\0'};
      gen_path_key(lookup_filename, topic, topic_len, ctx->key_depth,
                   ctx->key_bins);
      kvs_time.resumeTime();
      rc = dyad_kvs_read(ctx, topic, lookup_filename, false, &mdata);
      kvs_time.pauseTime();
      REQUIRE(rc >= 0);
    }
    AGGREGATE_TIME(kvs);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             args.number_of_files, total_kvs / info.comm_size,
             args.number_of_files * info.comm_size * info.comm_size /
                 total_kvs / 1000 / 1000);
    }
  }
  rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(posttest() == 0);
}
// clang-format off
TEST_CASE("RemoteKVSLookup",  "[number_of_lookups= " + std::to_string(args.number_of_files) +"]"
                              "[parallel_req= " + std::to_string(info.comm_size) +"]"
                              "[num_nodes= " + std::to_string(info.comm_size / args.process_per_node) +"]") {
  // clang-format on
  REQUIRE(pretest() == 0);
  dyad_rc_t rc = dyad_init_env(DYAD_COMM_RECV, info.flux_handle);
  REQUIRE(rc >= 0);
  auto ctx = dyad_ctx_get();
  SECTION("Throughput") {
    Timer kvs_time;
    char my_filename[4096], lookup_filename[4096];
    for (size_t file_idx = 0; file_idx < args.number_of_files; ++file_idx) {
      sprintf(my_filename, "%s/%s_%u_%d_%d_%zu.bat",
              args.dyad_managed_dir.c_str(), args.filename.c_str(),
              info.broker_idx, info.rank, info.comm_size, file_idx);
      sprintf(lookup_filename, "%s_%u_%d_%d_%zu.bat", args.filename.c_str(),
              info.broker_idx, info.rank, info.comm_size, file_idx);
      rc = dyad_commit(ctx, my_filename);
      REQUIRE(rc >= 0);
      dyad_metadata_t* mdata;
      const size_t topic_len = PATH_MAX;
      char topic[PATH_MAX + 1] = {'\0'};
      gen_path_key(lookup_filename, topic, topic_len, ctx->key_depth,
                   ctx->key_bins);
      kvs_time.resumeTime();
      rc = dyad_kvs_read(ctx, topic, lookup_filename, false, &mdata);
      kvs_time.pauseTime();
      REQUIRE(rc >= 0);
    }
    AGGREGATE_TIME(kvs);
    if (info.rank == 0) {
      printf("[DYAD_TEST],%10d,%10lu,%10.6f,%10.6f\n", info.comm_size,
             args.number_of_files, total_kvs / info.comm_size,
             args.number_of_files * info.comm_size * info.comm_size /
                 total_kvs / 1000 / 1000);
    }
  }
  rc = dyad_finalize();
  REQUIRE(rc >= 0);
  REQUIRE(posttest() == 0);
}
