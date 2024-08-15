#include <cstdlib>
#include <iostream>
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

using namespace P4::literals;  // NOLINT

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

    const P4::IR::P4Program *program = P4::parseP4File(options);
    if (program == nullptr || P4::errorCount() > 0) {
        return EXIT_FAILURE;
    }

    // Initialize our logger
    P4::ToZ3::Logger::init();

    z3::context ctx;
    try {
        P4::ToZ3::P4State state(&ctx);
        P4::ToZ3::Z3Visitor toZ3(&state, false);
        program->apply(toZ3);

        const auto *decl = P4::ToZ3::get_main_decl(&state);
        if (decl == nullptr) {
            return EXIT_SKIPPED;
        }
        P4::ToZ3::Z3Visitor toZ3Second(&state);
        auto declResult = gen_state_from_instance(&toZ3Second, decl);
        for (const auto &pipeState : declResult) {
            P4::cstring pipeName = pipeState.first;
            const auto pipeVars = pipeState.second.first;
            if (!pipeVars.empty()) {
                std::cout << "Pipe " << pipeName << " state:" << std::endl;
                for (const auto &tuple : pipeVars) {
                    auto name = tuple.first;
                    auto var = tuple.second.simplify();
                    std::cout << name << ": " << var << "\n";
                }
            } else {
                warning("No results for pipe %s", pipeName);
            }
        }
    } catch (const P4::Util::P4CExceptionBase &bug) {
        std::cerr << bug.what() << std::endl;
        return EXIT_FAILURE;
    } catch (z3::exception &ex) {
        std::cerr << "Z3 exception: " << ex << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
