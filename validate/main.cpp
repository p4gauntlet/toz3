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

std::vector<std::filesystem::path> generate_pass_list(const fs::path &p4_file,
                                                      const fs::path &dump_dir,
                                                      const fs::path &compiler_bin) {
    std::string cmd = compiler_bin;
    // FIXME: use absl::StrConcat
    cmd += " " + std::string(PASSES) + " ";
    cmd += std::string("--dump ") + dump_dir + " " + p4_file.c_str();
    cmd += " 2>&1";
    std::stringstream output;
    TOZ3::exec(cstring(cmd), output);
    std::vector<std::filesystem::path> pass_list;
    std::string cmd1 = compiler_bin.c_str();
    cmd1 += cstring(" --Wdisable  -v ") + p4_file.c_str();
    cmd1 += " 2>&1 ";
    cmd1 += "| sed -e '/FrontEnd\\|MidEnd\\|PassManager/!d' | ";
    cmd1 += "sed -e '/Writing program to/d' ";
    std::stringstream passes;
    TOZ3::exec(cstring(cmd1), passes);
    std::string pass;
    while (std::getline(passes, pass, '\n')) {
        cstring pass_path(
            (dump_dir / (cstring(p4_file.stem().c_str()) + "-" + pass + ".p4").c_str()).c_str());
        pass_list.emplace_back(pass_path.c_str());
    }

    if (pass_list.size() < 2) {
        return pass_list;
    }

    std::vector<std::filesystem::path> pruned_pass_list;
    auto it = pass_list.begin();
    auto pass_before = *it;
    pruned_pass_list.emplace_back(pass_before);
    std::advance(it, 1);
    for (; it != pass_list.end(); ++it) {
        auto pass_after = *it;
        if (TOZ3::compare_files(pass_before, pass_after)) {
            fs::remove(pass_after.c_str());
        } else {
            pruned_pass_list.emplace_back(pass_after);
            pass_before = pass_after;
        }
    }
    return pruned_pass_list;
}

int validate_translation(const fs::path &p4_file, const fs::path &dump_dir,
                         const fs::path &compiler_bin, ValidateOptions *options) {
    TOZ3::Logger::log_msg(0, "Analyzing %s", p4_file);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    auto prog_list = generate_pass_list(p4_file, dump_dir, compiler_bin);
    if (prog_list.size() < 2) {
        std::cerr << "P4 file did not generate enough passes." << std::endl;
        return EXIT_SKIPPED;
    }
    int result = TOZ3::process_programs(prog_list, options, options->undefined_is_ok);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto time_elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / SEC_TO_MS;
    TOZ3::Logger::log_msg(0, "Validation took %s seconds.", time_elapsed);
    return result;
}

int main(int argc, char *const argv[]) {
    AutoCompileContext autoP4toZ3Context(new P4toZ3Context);
    auto &options = P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test"_cs;

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0) {
        return EXIT_FAILURE;
    }

    // Initialize our logger
    TOZ3::Logger::init();

    auto p4_file = fs::path(options.file.c_str());
    auto dump_dir = options.dump_dir != nullptr ? fs::path(options.dump_dir.c_str()) : DUMP_DIR;
    dump_dir = dump_dir / p4_file.filename().stem();
    fs::create_directories(dump_dir);
    auto compiler_bin =
        options.compiler_bin != nullptr ? fs::path(options.compiler_bin.c_str()) : COMPILER_BIN;
    TOZ3::Logger::log_msg(0, "Using the compiler binary %s.", compiler_bin);

    return validate_translation(p4_file, dump_dir, compiler_bin, &options);
}
