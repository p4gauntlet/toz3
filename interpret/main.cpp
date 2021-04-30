#include <cstdio>
#include <fstream>

#include "frontends/common/applyOptionsPragmas.h"
#include "frontends/common/constantFolding.h"
#include "frontends/common/parseInput.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/createBuiltins.h"
#include "frontends/p4/directCalls.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/p4/frontend.h"
#include "frontends/p4/parseAnnotations.h"
#include "frontends/p4/specialize.h"
#include "frontends/p4/specializeGenericFunctions.h"
#include "frontends/p4/typeChecking/bindVariables.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/validateParsedProgram.h"

#include "frontends/p4/fromv1.0/v1model.h"
#include "frontends/p4/toP4/toP4.h"

#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/log.h"
#include "lib/nullstream.h"

#include "options.h"
#include "toz3/common/visitor_fill_type.h"
#include "toz3/common/visitor_interpret.h"

const IR::Declaration_Instance *get_main_decl(TOZ3::P4State *state) {
    const auto *main = state->find_static_decl("main");
    if (main != nullptr) {
        if (const auto *main_pkg = main->decl->to<IR::Declaration_Instance>()) {
            return main_pkg;
        }
        P4C_UNIMPLEMENTED("Main node %s not implemented!",
                          main->decl->node_type_name());
    }
    warning("No main declaration found in program.");
    return nullptr;
}

int main(int argc, char *const argv[]) {
    setup_gc_logging();

    AutoCompileContext autoP4toZ3Context(new TOZ3::P4toZ3Context);
    auto &options = TOZ3::P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test";

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0) {
        return 1;
    }

    auto hook = options.getDebugHook();

    const IR::P4Program *program = P4::parseP4File(options);

    if (program != nullptr && ::errorCount() == 0) {
        z3::context ctx;
        try {
            P4::P4COptionPragmaParser optionsPragmaParser;
            program->apply(P4::ApplyOptionsPragmas(optionsPragmaParser));
            // convert the P4 program to Z3 Python
            TOZ3::P4State state = TOZ3::P4State(&ctx);
            TOZ3::TypeVisitor map_builder = TOZ3::TypeVisitor(&state);
            TOZ3::Z3Visitor to_z3 = TOZ3::Z3Visitor(&state);
            program->apply(map_builder);

            const auto *decl = get_main_decl(&state);
            if (decl == nullptr) {
                return EXIT_SKIPPED;
            }
            auto decl_result = to_z3.gen_state_from_instance(decl);
            for (auto pipe_state : decl_result) {
                cstring pipe_name = pipe_state.first;
                const auto *pipe_vars =
                    pipe_state.second.first->to<TOZ3::ControlState>();
                if (pipe_vars != nullptr) {
                    std::cout << "Pipe " << pipe_name << " state:" << std::endl;
                    for (const auto &tuple : pipe_vars->state_vars) {
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
    }
    if (errorCount() > 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
