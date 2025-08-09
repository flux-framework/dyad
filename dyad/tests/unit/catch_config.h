//
// Created by haridev on 2/28/24.
//

#ifndef DYAD_CATCH_CONFIG_H
#define DYAD_CATCH_CONFIG_H
#include <catch2/catch_all.hpp>
#include "mpi.h"
#include <iostream>
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

#include <catch2/reporters/catch_reporter_streaming_base.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>

namespace Catch {
    // Fwd decls
    class TablePrinter;

    class MPIConsoleReporter final : public StreamingReporterBase {
        Detail::unique_ptr<TablePrinter> m_tablePrinter;

    public:
        MPIConsoleReporter(ReporterConfig&& config);
        ~MPIConsoleReporter() override;
        static std::string getDescription();

        void noMatchingTestCases( StringRef unmatchedSpec ) override;
        void reportInvalidTestSpec( StringRef arg ) override;

        void assertionStarting(AssertionInfo const&) override;

        void assertionEnded(AssertionStats const& _assertionStats) override;

        void sectionStarting(SectionInfo const& _sectionInfo) override;
        void sectionEnded(SectionStats const& _sectionStats) override;

        void benchmarkPreparing( StringRef name ) override;
        void benchmarkStarting(BenchmarkInfo const& info) override;
        void benchmarkEnded(BenchmarkStats<> const& stats) override;
        void benchmarkFailed( StringRef error ) override;

        void testCaseEnded(TestCaseStats const& _testCaseStats) override;
        void testRunEnded(TestRunStats const& _testRunStats) override;
        void testRunStarting(TestRunInfo const& _testRunInfo) override;

    private:
        void lazyPrint();

        void lazyPrintWithoutClosingBenchmarkTable();
        void lazyPrintRunInfo();
        void printTestCaseAndSectionHeader();

        void printClosedHeader(std::string const& _name);
        void printOpenHeader(std::string const& _name);

        // if string has a : in first line will set indent to follow it on
        // subsequent lines
        void printHeaderString(std::string const& _string, std::size_t indent = 0);

        void printTotalsDivider(Totals const& totals);

        bool m_headerPrinted = false;
        bool m_testRunInfoPrinted = false;
    };

} // end namespace Catch

#endif  // DYAD_CATCH_CONFIG_H
