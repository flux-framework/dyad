#include <dyad/cache/dyad_cache_api.h>
#include <dyad/cache/dyad_cache_int.h>
#include <dyad/common/dyad_structures_int.h>
#include <dyad/utils/range_cache.h>
#include <dyad/utils/utils.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

// Zero-initialized dyad_ctx_t with just enough set to exercise the cache
// module directly, without needing Flux/MPI (pure-function test tier).
struct dyad_ctx make_test_ctx ()
{
    struct dyad_ctx ctx;
    memset (&ctx, 0, sizeof (ctx));
    ctx.pid = getpid ();
    return ctx;
}

std::string make_temp_dir ()
{
    char tmpl[] = "/tmp/dyad_cache_test_XXXXXX";
    char *dir = mkdtemp (tmpl);
    REQUIRE (dir != nullptr);
    return std::string (dir);
}

void write_file (const std::string &path, size_t size_bytes)
{
    FILE *f = fopen (path.c_str (), "wb");
    REQUIRE (f != nullptr);
    std::string buf (size_bytes, 'x');
    REQUIRE (fwrite (buf.data (), 1, buf.size (), f) == buf.size ());
    fclose (f);
}

// utimensat() lets us set deterministic atime/mtime instead of relying on
// sleeps between writes (which would be both slow and flaky under coarse
// filesystem timestamp granularity).
void set_times (const std::string &path, time_t atime, time_t mtime)
{
    struct timespec times[2];
    times[0].tv_sec = atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = mtime;
    times[1].tv_nsec = 0;
    REQUIRE (utimensat (AT_FDCWD, path.c_str (), times, 0) == 0);
}

void remove_dir_recursive (const std::string &dir)
{
    std::string cmd = "rm -rf '" + dir + "'";
    system (cmd.c_str ());
}

}  // namespace

TEST_CASE ("dyad_cache_lru_recency_key", "[module=dyad_cache][method=get_recency_key]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    REQUIRE (dyad_cache_policy_init (&ctx, DYAD_CACHE_LRU) == DYAD_RC_OK);
    REQUIRE (ctx.cache_policy != nullptr);
    REQUIRE (ctx.cache_policy->get_recency_key != nullptr);

    struct stat st_old;
    memset (&st_old, 0, sizeof (st_old));
    st_old.st_atime = 100;
    struct stat st_new;
    memset (&st_new, 0, sizeof (st_new));
    st_new.st_atime = 200;

    int64_t key_old = 0, key_new = 0;
    REQUIRE (ctx.cache_policy->get_recency_key (&ctx, &st_old, &key_old) == DYAD_RC_OK);
    REQUIRE (ctx.cache_policy->get_recency_key (&ctx, &st_new, &key_new) == DYAD_RC_OK);
    REQUIRE (key_old == 100);
    REQUIRE (key_new == 200);
    REQUIRE (key_old < key_new);

    dyad_cache_policy_finalize (&ctx);
}

TEST_CASE ("dyad_cache_fifo_recency_key", "[module=dyad_cache][method=get_recency_key]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    REQUIRE (dyad_cache_policy_init (&ctx, DYAD_CACHE_FIFO) == DYAD_RC_OK);
    REQUIRE (ctx.cache_policy != nullptr);

    struct stat st_old;
    memset (&st_old, 0, sizeof (st_old));
    st_old.st_mtime = 50;
    struct stat st_new;
    memset (&st_new, 0, sizeof (st_new));
    st_new.st_mtime = 150;

    int64_t key_old = 0, key_new = 0;
    REQUIRE (ctx.cache_policy->get_recency_key (&ctx, &st_old, &key_old) == DYAD_RC_OK);
    REQUIRE (ctx.cache_policy->get_recency_key (&ctx, &st_new, &key_new) == DYAD_RC_OK);
    REQUIRE (key_old == 50);
    REQUIRE (key_new == 150);

    dyad_cache_policy_finalize (&ctx);
}

TEST_CASE ("dyad_cache_scan_dir", "[module=dyad_cache][method=scan]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    REQUIRE (dyad_cache_policy_init (&ctx, DYAD_CACHE_LRU) == DYAD_RC_OK);
    std::string dir = make_temp_dir ();

    write_file (dir + "/a.bin", 100);
    write_file (dir + "/b.bin", 200);
    // The lock file must be excluded from scan results even though it lives
    // in the same managed directory.
    write_file (dir + "/" + DYAD_CACHE_LOCK_FILENAME, 999);

    struct dyad_cache_entry *entries = nullptr;
    size_t n_entries = 0;
    uint64_t total_size = 0;
    REQUIRE (dyad_cache_scan_dir (&ctx, dir.c_str (), &entries, &n_entries, &total_size)
             == DYAD_RC_OK);
    REQUIRE (n_entries == 2);
    REQUIRE (total_size == 300);

    free (entries);
    dyad_cache_policy_finalize (&ctx);
    remove_dir_recursive (dir);
}

TEST_CASE ("dyad_cache_maybe_evict_noop_when_disabled", "[module=dyad_cache][method=maybe_evict]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    ctx.cache_capacity_bytes = 0;  // disabled (the default)
    ctx.cache_policy_mode = DYAD_CACHE_NONE;
    std::string dir = make_temp_dir ();
    write_file (dir + "/a.bin", 1024);

    REQUIRE (dyad_cache_maybe_evict (&ctx, dir.c_str ()) == DYAD_RC_OK);
    REQUIRE (access ((dir + "/a.bin").c_str (), F_OK) == 0);
    // No-op must not even create the lock file.
    REQUIRE (access ((dir + "/" + DYAD_CACHE_LOCK_FILENAME).c_str (), F_OK) != 0);

    remove_dir_recursive (dir);
}

TEST_CASE ("dyad_cache_maybe_evict_selects_oldest_first", "[module=dyad_cache][method=maybe_evict]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    REQUIRE (dyad_cache_policy_init (&ctx, DYAD_CACHE_LRU) == DYAD_RC_OK);
    ctx.cache_policy_mode = DYAD_CACHE_LRU;
    ctx.cache_capacity_bytes = 2048;     // 2 KiB cap
    ctx.cache_low_watermark_frac = 0.5;  // evict down to 1 KiB
    ctx.cache_grace_period_sec = 0;      // don't skip anything based on recency

    std::string dir = make_temp_dir ();
    write_file (dir + "/oldest.bin", 1024);
    write_file (dir + "/middle.bin", 1024);
    write_file (dir + "/newest.bin", 1024);
    // Deterministic ages: oldest.bin < middle.bin < newest.bin
    set_times (dir + "/oldest.bin", 100, 100);
    set_times (dir + "/middle.bin", 200, 200);
    set_times (dir + "/newest.bin", 300, 300);

    REQUIRE (dyad_cache_maybe_evict (&ctx, dir.c_str ()) == DYAD_RC_OK);

    REQUIRE (access ((dir + "/oldest.bin").c_str (), F_OK) != 0);
    REQUIRE (access ((dir + "/newest.bin").c_str (), F_OK) == 0);

    dyad_cache_policy_finalize (&ctx);
    remove_dir_recursive (dir);
}

TEST_CASE ("dyad_cache_maybe_evict_skips_locked_candidate",
           "[module=dyad_cache][method=maybe_evict]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    REQUIRE (dyad_cache_policy_init (&ctx, DYAD_CACHE_LRU) == DYAD_RC_OK);
    ctx.cache_policy_mode = DYAD_CACHE_LRU;
    ctx.cache_capacity_bytes = 1024;  // 1 KiB cap
    ctx.cache_low_watermark_frac = 0.5;
    ctx.cache_grace_period_sec = 0;

    std::string dir = make_temp_dir ();
    std::string locked_path = dir + "/locked_oldest.bin";
    std::string free_path = dir + "/free_newest.bin";
    write_file (locked_path, 1024);
    write_file (free_path, 1024);
    set_times (locked_path, 100, 100);  // oldest -- would normally be evicted first
    set_times (free_path, 200, 200);

    // Hold a real exclusive flock on locked_path from a child process, since
    // fcntl locks are associated with the (process, open file description),
    // not the fd alone -- a lock from this same process wouldn't conflict.
    pid_t child = fork ();
    REQUIRE (child >= 0);
    if (child == 0) {
        int fd = open (locked_path.c_str (), O_RDWR);
        if (fd == -1) {
            _exit (1);
        }
        struct flock lock;
        memset (&lock, 0, sizeof (lock));
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        if (fcntl (fd, F_SETLK, &lock) == -1) {
            _exit (1);
        }
        // Hold the lock until the parent signals it's done checking.
        pause ();
        _exit (0);
    }
    // Give the child a moment to acquire the lock.
    usleep (200000);

    REQUIRE (dyad_cache_maybe_evict (&ctx, dir.c_str ()) == DYAD_RC_OK);

    // The locked (oldest) file must survive; eviction should have skipped it.
    REQUIRE (access (locked_path.c_str (), F_OK) == 0);

    kill (child, SIGTERM);
    int status = 0;
    waitpid (child, &status, 0);

    dyad_cache_policy_finalize (&ctx);
    remove_dir_recursive (dir);
}

TEST_CASE ("dyad_range_cache_clamps_request_to_origin_size",
           "[module=dyad_cache][method=range_cache]")
{
    struct dyad_ctx ctx = make_test_ctx ();
    const size_t origin_size = 2ul * DYAD_RANGE_CACHE_BLOCK_SIZE;
    std::string dir = make_temp_dir ();
    std::string origin_path = dir + "/origin.bin";
    std::string local_path = dir + "/local.bin";
    struct stat local_st;

    write_file (origin_path, origin_size);
    REQUIRE (dyad_range_cache_ensure (&ctx,
                                      local_path.c_str (),
                                      origin_path.c_str (),
                                      origin_size - 10ul,
                                      10ul * DYAD_RANGE_CACHE_BLOCK_SIZE)
             == DYAD_RC_OK);
    REQUIRE (stat (local_path.c_str (), &local_st) == 0);
    REQUIRE ((size_t)local_st.st_size == origin_size);

    remove_dir_recursive (dir);
}
