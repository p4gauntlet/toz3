
#include "toz3/validate/passes.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "toz3/common/exceptions.h"
#include "toz3/common/util.h"

namespace P4::ToZ3 {
namespace {

struct PassGenerationPaths {
    std::filesystem::path root;
    std::filesystem::path input;
    std::filesystem::path dump;
    std::filesystem::path compiler;
};

class PassGeneration : public testing::Test, protected PassGenerationPaths {
 protected:
    void SetUp() override {
        std::string pathTemplate = "/tmp/toz3-tests-XXXXXX";
        const auto *temp = mkdtemp(pathTemplate.data());
        ASSERT_NE(temp, nullptr);
        root = temp;
        const auto specialDir = root / "paths with spaces ' \" \\ $ ;";
        std::filesystem::create_directory(specialDir);
        input = specialDir / "input.p4";
        dump = specialDir / "dump";
        compiler = specialDir / "mock compiler";
        std::filesystem::create_directory(dump);
        std::ofstream(compiler) << R"PY(#!/usr/bin/env python3
import sys
from pathlib import Path

args = sys.argv[1:]
mode = Path(args[-1]).read_text()
verbose = '-v' in args
if mode == 'fail-list' or (mode == 'fail-dump' and not verbose):
    print('mock compiler failure', file=sys.stderr)
    sys.exit(7)
passes = ['FrontEnd_0_First', 'FrontEnd_1_Last', 'MidEnd_0_Last']
if mode == 'many':
    passes = ['FrontEnd_0_First', 'FrontEnd_1_Rewrite(x)+', 'FrontEnd_2_Last(x)+',
              'MidEnd_0_First', 'MidEnd_1_Rewrite', 'MidEnd_2_Last',
              'PassManager_0_Prepare', 'PassManager_1_Final']
if mode == 'too-few':
    passes = passes[:1]
if verbose:
    assert '--top4' not in args, 'Discovery should not dump programs'
    print('\n'.join(passes), file=sys.stderr)
    print('unrelated compiler diagnostic', file=sys.stderr)
    print('Writing program to "ignored-OtherPass.p4"', file=sys.stderr)
else:
    dump = Path(args[args.index('--dump') + 1])
    selectors = args[args.index('--top4') + 1].split(',')
    assert all(s.startswith('^') and s.endswith('_') for s in selectors)
    source = Path(args[-1])
    sequence = 0
    for index, name in enumerate(passes):
        if not any(name.startswith(s[1:]) for s in selectors):
            continue
        sequence += 1
        path = dump / f'{source.stem}-{sequence:04}-{name}{source.suffix}'
        path.write_text(name if mode == 'many' else
                        'same' if index < 2 or mode == 'identical' else 'different')
    if mode == 'missing':
        path.unlink()
)PY";
        std::filesystem::permissions(compiler, std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::add);
    }

    void TearDown() override {
        if (!root.empty()) {
            std::filesystem::remove_all(root);
        }
    }

    std::vector<std::filesystem::path> generate(const std::string &mode,
                                                PassSelection selection = PassSelection::All) {
        std::ofstream(input) << mode;
        return generatePassList(input, dump, compiler, selection);
    }
};

TEST_F(PassGeneration, QuotesPathsAndPrunesConsecutiveDuplicates) {
    const auto passes = generate("different");
    ASSERT_EQ(passes.size(), 2U);
    EXPECT_EQ(passes.front(), dump / "input-0001-FrontEnd_0_First.p4");
    EXPECT_EQ(passes.back(), dump / "input-0003-MidEnd_0_Last.p4");
    EXPECT_FALSE(std::filesystem::exists(dump / "input-0002-FrontEnd_1_Last.p4"));
}

TEST_F(PassGeneration, KeepsOnePassWhenAllDumpsAreIdentical) {
    const auto passes = generate("identical");
    ASSERT_EQ(passes.size(), 1U);
    EXPECT_EQ(passes.front(), dump / "input-0001-FrontEnd_0_First.p4");
}

TEST_F(PassGeneration, ReturnsSinglePassWithoutComparison) {
    EXPECT_EQ(generate("too-few").size(), 1U);
}

TEST_F(PassGeneration, ReportsCompilerFailureDuringPassListing) {
    try {
        generate("fail-list");
        FAIL() << "Expected a compiler execution error";
    } catch (const CompilerExecutionError &error) {
        EXPECT_NE(std::string(error.what()).find("mock compiler failure"), std::string::npos);
    }
}

TEST_F(PassGeneration, ReportsCompilerFailureDuringDumping) {
    EXPECT_THROW(generate("fail-dump"), CompilerExecutionError);
}

TEST_F(PassGeneration, ReportsMissingDump) { EXPECT_THROW(generate("missing"), GauntletException); }

TEST_F(PassGeneration, EmitsOnlyInitialPhaseBoundariesAndFinalPass) {
    const auto passes = generate("many", PassSelection::Checkpoints);
    ASSERT_EQ(passes.size(), 4U);
    EXPECT_EQ(passes[0].filename(), "input-0001-FrontEnd_0_First.p4");
    EXPECT_EQ(passes[1].filename(), "input-0002-FrontEnd_2_Last(x)+.p4");
    EXPECT_EQ(passes[2].filename(), "input-0003-MidEnd_2_Last.p4");
    EXPECT_EQ(passes[3].filename(), "input-0004-PassManager_1_Final.p4");
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(dump),
                            std::filesystem::directory_iterator()),
              4);
}

TEST_F(PassGeneration, PassingCheckpointsAvoidIndividualPassDumps) {
    std::ofstream(input) << "many";
    unsigned comparisons = 0;
    EXPECT_EQ(validatePasses(input, dump, compiler,
                             [&](const auto &passes, bool diagnostics) {
                                 ++comparisons;
                                 EXPECT_EQ(passes.size(), 4U);
                                 EXPECT_FALSE(diagnostics);
                                 return EXIT_SUCCESS;
                             }),
              EXIT_SUCCESS);
    EXPECT_EQ(comparisons, 1U);
    EXPECT_FALSE(std::filesystem::exists(dump / "passes"));
}

TEST_F(PassGeneration, ViolationsTriggerDetailedPassDiagnostics) {
    std::ofstream(input) << "many";
    unsigned comparisons = 0;
    EXPECT_EQ(validatePasses(input, dump, compiler,
                             [&](const auto &passes, bool diagnostics) {
                                 ++comparisons;
                                 EXPECT_EQ(passes.size(), comparisons == 1 ? 4U : 8U);
                                 EXPECT_EQ(diagnostics, comparisons == 2);
                                 return EXIT_VIOLATION;
                             }),
              EXIT_VIOLATION);
    EXPECT_EQ(comparisons, 2U);
}

TEST_F(PassGeneration, ExhaustiveModeBypassesTheCheckpointCheck) {
    std::ofstream(input) << "many";
    unsigned comparisons = 0;
    EXPECT_EQ(validatePasses(
                  input, dump, compiler,
                  [&](const auto &passes, bool diagnostics) {
                      ++comparisons;
                      EXPECT_EQ(passes.size(), 8U);
                      EXPECT_TRUE(diagnostics);
                      return EXIT_SUCCESS;
                  },
                  true),
              EXIT_SUCCESS);
    EXPECT_EQ(comparisons, 1U);
    EXPECT_FALSE(std::filesystem::exists(dump / "checkpoints"));
}

TEST_F(PassGeneration, DetailedCheckDeterminesTheFinalResult) {
    std::ofstream(input) << "many";
    unsigned comparisons = 0;
    EXPECT_EQ(validatePasses(input, dump, compiler,
                             [&](const auto &passes, bool diagnostics) {
                                 ++comparisons;
                                 EXPECT_EQ(diagnostics, comparisons != 1);
                                 EXPECT_EQ(passes.size(), comparisons == 1 ? 4U : 8U);
                                 return comparisons == 1 ? EXIT_VIOLATION : EXIT_SUCCESS;
                             }),
              EXIT_SUCCESS);
    EXPECT_EQ(comparisons, 2U);
}

}  // namespace
}  // namespace P4::ToZ3
