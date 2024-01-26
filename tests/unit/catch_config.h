#ifndef DYAD_CATCH_CONFIG_H
#define DYAD_CATCH_CONFIG_H
#define CATCH_CONFIG_RUNNER
#include <cpp-logger/logger.h>

#include <catch2/catch_all.hpp>

namespace cl = Catch::Clara;

cl::Parser define_options();

int init(int *argc, char ***argv);
int finalize();

#define CONVERT_ENUM(name, value) \
  "[" + std::string(#name) + "=" + std::to_string(static_cast<int>(value)) + "]"

#define CONVERT_VAL(name, value) \
  "[" + std::string(#name) + "=" + std::to_string(value) + "]"
#define CONVERT_STR(name, value) \
  "[" + std::string(#name) + "=" + std::string(value) + "]"

#define DEFINE_CLARA_OPS(TYPE)                                 \
  std::ostream &operator<<(std::ostream &out, const TYPE &c) { \
    out << static_cast<int>(c) << std::endl;                   \
    return out;                                                \
  }                                                            \
  TYPE &operator>>(const std::stringstream &out, TYPE &c) {    \
    c = static_cast<TYPE>(atoi(out.str().c_str()));            \
    return c;                                                  \
  }
#define AGGREGATE_TIME(name)                                      \
  double total_##name = 0.0;                                      \
  auto name##_a = name##_time.getElapsedTime();                   \
  MPI_Reduce(&name##_a, &total_##name, 1, MPI_DOUBLE, MPI_SUM, 0, \
             MPI_COMM_WORLD);
#define TEST_LOGGER cpplogger::Logger::Instance("DYAD_TEST")

#define TEST_LOGGER_INFO(...) \
  cpplogger::Logger::Instance("DYAD_TEST")  \
      ->log(cpplogger::LOG_INFO, __VA_ARGS__);

#define TEST_LOGGER_ERROR(...) \
  cpplogger::Logger::Instance("DYAD_TEST")  \
      ->log(cpplogger::LOG_ERROR, __VA_ARGS__);

#define PRINT_MSG(...)            \
  cpplogger::Logger::Instance("DYAD_TEST") \
      ->log(cpplogger::LOG_PRINT, __VA_ARGS__);


int main(int argc, char *argv[]) {
    Catch::Session session;
    auto cli = session.cli() | define_options();
    session.cli(cli);
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) return returnCode;
    returnCode = init(&argc, &argv);
    if (returnCode != 0) return returnCode;
    int test_return_code = session.run();
    returnCode = finalize();
    if (returnCode != 0) return returnCode;
    exit(test_return_code);
}
#endif  // DYAD_CATCH_CONFIG_H
