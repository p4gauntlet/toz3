#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "../common/exceptions.h"
#include "../common/util.h"
#include "../compare/compare.h"
#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"
#include "lib/compile_context.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "options.h"
#include "passes.h"

using namespace P4::literals;  // NOLINT

namespace fs = std::filesystem;

static const auto FILE_DIR = fs::path(__FILE__).parent_path();
static const auto COMPILER_BIN = FILE_DIR / "../../../../p4c/build/p4test";
static const auto DUMP_DIR = fs::path("validated");

static constexpr auto SEC_TO_MS = 1000000.0;

namespace P4::ToZ3 {

int validateTranslation(const fs::path &p4_file, const fs::path &dump_dir,
                        const fs::path &compiler_bin, ValidateOptions *options) {
    Logger::log_msg(0, "Analyzing %s", p4_file);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    try {
        auto progList = generatePassList(p4_file, dump_dir, compiler_bin);
        if (progList.size() < 2) {
            std::cerr << "P4 file did not generate enough passes to compare." << std::endl;
            return EXIT_SKIPPED;
        }
        int result = process_programs(progList, options, options->undefined_is_ok);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        auto timeElapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / SEC_TO_MS;
        Logger::log_msg(0, "Validation took %s seconds.", timeElapsed);
        return result;
    } catch (const GauntletException &e) {
        std::cerr << "\nValidation failed with an error:\n" << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
}  // namespace P4::ToZ3

int main(int argc, char *const argv[]) {
    P4::AutoCompileContext autoP4toZ3Context(new P4::ToZ3::P4toZ3Context);
    auto &options = P4::ToZ3::P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = P4::CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test"_cs;

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (P4::errorCount() > 0) {
        return EXIT_FAILURE;
    }

    // Initialize our logger
    P4::ToZ3::Logger::init();

    auto p4File = fs::path(options.file.c_str());
    auto dumpDir = options.dump_dir != nullptr ? fs::path(options.dump_dir.c_str()) : DUMP_DIR;
    dumpDir = dumpDir / p4File.filename().stem();
    fs::create_directories(dumpDir);
    auto compilerBin =
        options.compiler_bin != nullptr ? fs::path(options.compiler_bin.c_str()) : COMPILER_BIN;
    P4::ToZ3::Logger::log_msg(0, "Using the compiler binary %s.", compilerBin);

    return P4::ToZ3::validateTranslation(p4File, dumpDir, compilerBin, &options);
}
