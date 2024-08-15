
#include <cstdlib>
#include <iostream>
#include <vector>

#include "compare.h"
#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"
#include "lib/compile_context.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "toz3/common/util.h"
#include "toz3/compare/options.h"

using namespace P4::literals;  // NOLINT

std::vector<std::filesystem::path> splitInputProgs(P4::cstring inputProgs) {
    std::vector<std::filesystem::path> progList;
    const char *pos = nullptr;
    P4::cstring prog;

    // FIXME: use absl::Split
    while ((pos = inputProgs.find(static_cast<size_t>(','))) != nullptr) {
        auto idx = static_cast<size_t>(pos - inputProgs);
        prog = inputProgs.substr(0, idx);
        progList.emplace_back(prog.c_str());
        inputProgs = inputProgs.substr(idx + 1);
    }
    progList.emplace_back(inputProgs.c_str());
    return progList;
}

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
    // Initialize our logger.
    P4::ToZ3::Logger::init();

    // Check the input file.
    if (options.file.empty()) {
        options.usage();
        return EXIT_FAILURE;
    }
    auto progList = splitInputProgs(P4::cstring(options.file.c_str()));
    if (progList.size() < 2) {
        std::cerr << "At least two input programs expected." << std::endl;
        options.usage();
        return EXIT_FAILURE;
    }
    return P4::ToZ3::process_programs(progList, &options, options.undefined_is_ok);
}
