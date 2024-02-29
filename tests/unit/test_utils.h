#ifndef DYAD_TEST_UTILS_H
#define DYAD_TEST_UTILS_H

#include <cmath>
#include <cstdio>
#include <string>
#include <unistd.h>

const uint32_t KB = 1024;
const uint32_t MB = 1024 * 1024;
#define AGGREGATE_TIME(name)                                      \
  double total_##name = 0.0;                                      \
  auto name##_a = name##_time.getElapsedTime();                   \
  MPI_Reduce(&name##_a, &total_##name, 1, MPI_DOUBLE, MPI_SUM, 0,  MPI_COMM_WORLD);

size_t GetRandomOffset(size_t i, unsigned int offset_seed, size_t stride,
                       size_t total_size) {
    return abs((int)(((i * rand_r(&offset_seed)) % stride) % total_size));
}
inline std::string get_filename(int fd) {
    const int kMaxSize = 256;
    char proclnk[kMaxSize];
    char filename[kMaxSize];
    snprintf(proclnk, kMaxSize, "/proc/self/fd/%d", fd);
    size_t r = readlink(proclnk, filename, kMaxSize);
    filename[r] = '\0';
    return filename;
}

std::string GenRandom(const int len) {
    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand(100);

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    tmp_s[len - 1] = '\n';

    return tmp_s;
}

class Timer {
    public:
    Timer() : elapsed_time(0) {}
    void resumeTime() { t1 = std::chrono::high_resolution_clock::now(); }
    double pauseTime() {
        auto t2 = std::chrono::high_resolution_clock::now();
        elapsed_time += std::chrono::duration<double>(t2 - t1).count();
        return elapsed_time;
    }
    double getElapsedTime() { return elapsed_time; }

    private:
    std::chrono::high_resolution_clock::time_point t1;
    double elapsed_time;
};

#endif  // DYAD_TEST_UTILS_H
