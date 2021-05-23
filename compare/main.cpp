
#include "frontends/common/parseInput.h"

#include "compare.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/visitor_interpret.h"

std::vector<cstring> split_input_progs(cstring input_progs) {
    std::vector<cstring> prog_list;
    const char *pos = nullptr;
    cstring prog;

    while ((pos = input_progs.find((size_t)',')) != nullptr) {
        auto idx = (size_t)(pos - input_progs);
        prog = input_progs.substr(0, idx);
        prog_list.push_back(prog);
        input_progs = input_progs.substr(idx + 1);
    }
    prog_list.push_back(input_progs);
    return prog_list;
}

int main(int argc, char *const argv[]) {
    AutoCompileContext autoP4toZ3Context(new TOZ3::P4toZ3Context);
    auto &options = TOZ3::P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test";

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0) {
        return EXIT_FAILURE;
    }
    // Initialize our logger
    TOZ3::Logger::init();

    // check input file
    if (options.file == nullptr) {
        options.usage();
        return EXIT_FAILURE;
    }
    auto prog_list = split_input_progs(options.file);
    if (prog_list.size() < 2) {
        std::cerr << "At least two input programs expected." << std::endl;
        options.usage();
        return EXIT_FAILURE;
    }
    return TOZ3::process_programs(prog_list, &options);
}
