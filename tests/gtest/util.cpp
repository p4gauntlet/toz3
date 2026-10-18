#include "toz3/common/util.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "toz3/common/exceptions.h"

namespace P4::ToZ3 {
namespace {

struct FileComparisonPaths {
    std::filesystem::path dump;
};

class FileComparison : public testing::Test, protected FileComparisonPaths {
 protected:
    void SetUp() override {
        std::string pathTemplate = "/tmp/toz3-files-XXXXXX";
        const auto *temp = mkdtemp(pathTemplate.data());
        ASSERT_NE(temp, nullptr);
        dump = temp;
    }

    void TearDown() override {
        if (!dump.empty()) {
            std::filesystem::remove_all(dump);
        }
    }
};

TEST_F(FileComparison, MissingFilesAreNotEqual) {
    EXPECT_THROW(compare_files(dump / "missing1", dump / "missing2"), GauntletException);
}

TEST_F(FileComparison, ComparesEmptyAndDifferentFiles) {
    const auto first = dump / "first";
    const auto second = dump / "second";
    {
        std::ofstream firstFile{first};
        std::ofstream secondFile{second};
    }
    EXPECT_TRUE(compare_files(first, second));
    std::ofstream(first) << "ab";
    std::ofstream(second) << "ac";
    EXPECT_FALSE(compare_files(first, second));
    std::ofstream(second) << "abc";
    EXPECT_FALSE(compare_files(first, second));
}

}  // namespace
}  // namespace P4::ToZ3
