
#include "toz3/validate/passes.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "toz3/common/exceptions.h"

namespace P4::ToZ3 {
namespace {

class PassGeneration : public testing::Test {
 protected:
    std::filesystem::path root;
    std::filesystem::path input;
    std::filesystem::path dump;
    std::filesystem::path compiler;

    void SetUp() override {
        char pathTemplate[] = "/tmp/toz3-tests-XXXXXX";
        const auto *temp = mkdtemp(pathTemplate);
        ASSERT_NE(temp, nullptr);
        root = temp;
        const auto specialDir = root / "paths with spaces ' \" \\ $ ;";
        std::filesystem::create_directory(specialDir);
        input = specialDir / "input.p4";
        dump = specialDir / "dump";
        compiler = specialDir / "mock compiler";
        std::filesystem::create_directory(dump);
        std::ofstream(compiler) << R"PY(#!/usr/bin/env python3
import json
import sys
from pathlib import Path

args = sys.argv[1:]
mode = Path(args[-1]).read_text()
verbose = '-v' in args
if mode == 'fail-list' or (mode == 'fail-dump' and not verbose):
    print('mock compiler failure', file=sys.stderr)
    sys.exit(7)
dump = Path(args[args.index('--dump') + 1])
passes = [dump / 'sample-0001-FrontEnd.p4',
          dump / 'sample-0002-FrontEnd.p4',
          dump / 'sample-0003-MidEnd.p4']
if mode == 'too-few':
    passes = passes[:1]
for index, path in enumerate(passes):
    path.write_text('same' if index < 2 or mode == 'identical' else 'different')
    if verbose:
        print('Writing program to ' + json.dumps(str(path)), file=sys.stderr)
if verbose:
    print('unrelated compiler diagnostic', file=sys.stderr)
    print('Writing program to "ignored-OtherPass.p4"', file=sys.stderr)
if mode == 'missing' and not verbose:
    passes[-1].unlink()
)PY";
        std::filesystem::permissions(compiler, std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::add);
    }

    void TearDown() override {
        if (!root.empty()) {
            std::filesystem::remove_all(root);
        }
    }

    std::vector<std::filesystem::path> generate(const std::string &mode) {
        std::ofstream(input) << mode;
        return generatePassList(input, dump, compiler);
    }
};

TEST_F(PassGeneration, QuotesPathsAndPrunesConsecutiveDuplicates) {
    const auto passes = generate("different");
    ASSERT_EQ(passes.size(), 2U);
    EXPECT_EQ(passes.front(), dump / "sample-0001-FrontEnd.p4");
    EXPECT_EQ(passes.back(), dump / "sample-0003-MidEnd.p4");
    EXPECT_FALSE(std::filesystem::exists(dump / "sample-0002-FrontEnd.p4"));
}

TEST_F(PassGeneration, KeepsOnePassWhenAllDumpsAreIdentical) {
    const auto passes = generate("identical");
    ASSERT_EQ(passes.size(), 1U);
    EXPECT_EQ(passes.front(), dump / "sample-0001-FrontEnd.p4");
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

}  // namespace
}  // namespace P4::ToZ3
