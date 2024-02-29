//
// Created by haridev on 2/28/24.
//

#ifndef DYAD_CATCH_CONFIG_H
#define DYAD_CATCH_CONFIG_H
#include <catch2/catch_all.hpp>
namespace cl = Catch::Clara;

cl::Parser define_options();

int init(int* argc, char*** argv);
int finalize();

int main(int argc, char* argv[]) {
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
