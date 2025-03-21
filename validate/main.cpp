#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "../common/util.h"
#include "../compare/compare.h"
#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"
#include "lib/compile_context.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "options.h"

using namespace P4::literals;

namespace fs = std::filesystem;

static const auto FILE_DIR = fs::path(__FILE__).parent_path();
static const auto COMPILER_BIN = FILE_DIR / "../../../../p4c/build/p4test";
static const auto DUMP_DIR = fs::path("validated");

static constexpr auto PASSES = "--top4 FrontEnd,MidEnd,PassManager ";
static constexpr auto SEC_TO_MS = 1000000.0;

namespace P4::ToZ3 {

std::vector<std::filesystem::path> generatePassList(const fs::path &p4_file,
                                                    const fs::path &dump_dir,
                                                    const fs::path &compiler_bin) {
    // FIXME: use absl::StrConcat
    // First, get the pass names.
    std::string passListCmd = compiler_bin.c_str();
    passListCmd += " " + std::string(PASSES) + " --dump " + dump_dir.string() + " ";
    passListCmd += " --Wdisable  -v " + p4_file.string();
    passListCmd += " 2>&1 ";
    passListCmd += R"#(| sed -e '/FrontEnd\|MidEnd\|PassManager/!d' )#";
    passListCmd += R"#(| grep 'Writing program to' )#";
    passListCmd += R"#(| sed -n 's/.*"\(.*\)".*/\1/p' )#";
    std::stringstream passes;
    exec(passListCmd.c_str(), passes);

    std::string pass;
    std::vector<std::filesystem::path> passList;
    while (std::getline(passes, pass, '\n')) {
        passList.emplace_back(pass);
    }

    if (passList.size() < 2) {
        return passList;
    }
    // Then, write the actual programs. Do not use -v here to avoid polluting the dumped files.
    std::string dumpCmd = compiler_bin.c_str();
    dumpCmd += " " + std::string(PASSES) + " ";
    dumpCmd += "--dump " + dump_dir.string() + " " + p4_file.c_str();
    dumpCmd += " 2>&1";
    std::stringstream output;
    exec(dumpCmd.c_str(), output);

    // Now, we remove all the dumped programs where no change was made.
    std::vector<std::filesystem::path> prunedPassList;
    auto it = passList.begin();
    auto passBefore = *it;
    prunedPassList.emplace_back(passBefore);
    std::advance(it, 1);
    for (; it != passList.end(); ++it) {
        auto passAfter = *it;
        if (compare_files(passBefore, passAfter)) {
            fs::remove(passAfter.c_str());
        } else {
            prunedPassList.emplace_back(passAfter);
            passBefore = passAfter;
        }
    }
    return prunedPassList;
}

int validateTranslation(const fs::path &p4_file, const fs::path &dump_dir,
                        const fs::path &compiler_bin, ValidateOptions *options) {
    Logger::log_msg(0, "Analyzing %s", p4_file);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    auto progList = generatePassList(p4_file, dump_dir, compiler_bin);
    if (progList.size() < 2) {
        std::cerr << "P4 file did not generate enough passes." << std::endl;
        return EXIT_SKIPPED;
    }
    int result = process_programs(progList, options, options->undefined_is_ok);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto timeElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / SEC_TO_MS;
    Logger::log_msg(0, "Validation took %s seconds.", timeElapsed);
    return result;
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
