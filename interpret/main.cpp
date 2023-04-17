#include <cstdlib>
#include <iostream>
#include <list>
#include <utility>
#include <vector>

#include "frontends/common/options.h"
#include "frontends/common/parseInput.h"
#include "frontends/common/parser_options.h"
#include "ir/ir.h"
#include "lib/compile_context.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "options.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/state.h"
#include "toz3/common/util.h"
#include "toz3/common/visitor_interpret.h"
#include "z3++.h"

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

    const IR::P4Program *program = P4::parseP4File(options);
    if (program == nullptr || ::errorCount() > 0) {
        return EXIT_FAILURE;
    }

    // Initialize our logger
    TOZ3::Logger::init();

    z3::context ctx;
    try {
        TOZ3::P4State state(&ctx);
        TOZ3::Z3Visitor to_z3(&state, false);
        program->apply(to_z3);

        const auto *decl = TOZ3::get_main_decl(&state);
        if (decl == nullptr) {
            return EXIT_SKIPPED;
        }
        TOZ3::Z3Visitor to_z3_second(&state);
        auto decl_result = gen_state_from_instance(&to_z3_second, decl);
        for (const auto &pipe_state : decl_result) {
            cstring pipe_name = pipe_state.first;
            const auto pipe_vars = pipe_state.second.first;
            if (!pipe_vars.empty()) {
                std::cout << "Pipe " << pipe_name << " state:" << std::endl;
                for (const auto &tuple : pipe_vars) {
                    auto name = tuple.first;
                    auto var = tuple.second.simplify();
                    std::cout << name << ": " << var << "\n";
                }
            } else {
                warning("No results for pipe %s", pipe_name);
            }
        }
    } catch (const Util::P4CExceptionBase &bug) {
        std::cerr << bug.what() << std::endl;
        return EXIT_FAILURE;
    } catch (z3::exception &ex) {
        std::cerr << "Z3 exception: " << ex << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
