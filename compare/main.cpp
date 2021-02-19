#include <cstdio>
#include <cstdlib>
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

#include "ir/ir-generated.h"
#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/log.h"
#include "lib/nullstream.h"

#include "options.h"
#include "toz3_v2/common/state.h"
#include "toz3_v2/common/type_map.h"
#include "toz3_v2/common/z3_interpreter.h"

namespace TOZ3_V2 {

const IR::Declaration_Instance *get_main_decl(P4State *state) {
    const IR::Declaration *main = state->get_decl("main");
    if (auto main_pkg = main->to<IR::Declaration_Instance>()) {
        return main_pkg;
    } else {
        BUG("Main node %s not implemented!", main->node_type_name());
    }
}

P4Z3Result get_z3_repr(const IR::P4Program *program, z3::context *ctx) {
    P4Z3Result z3_return;

    if (program != nullptr && ::errorCount() == 0) {
        try {
            P4::P4COptionPragmaParser optionsPragmaParser;
            program->apply(P4::ApplyOptionsPragmas(optionsPragmaParser));
            // convert the P4 program to Z3
            P4State *state = new P4State(ctx);
            TypeVisitor *map_builder = new TypeVisitor(state);
            Z3Visitor *to_z3 = new Z3Visitor(state);
            program->apply(*map_builder);
            auto decl = get_main_decl(state);
            decl->apply(*to_z3);
            P4Z3Result decl_result = to_z3->get_decl_result();
            return to_z3->get_decl_result();
        } catch (const Util::P4CExceptionBase &bug) {
            std::cerr << bug.what() << std::endl;
        }
    }
    return z3_return;
}

void unroll_result(P4Z3Result z3_repr_prog, std::vector<z3::expr> *result_vec) {
    for (auto result_tuple : z3_repr_prog) {
        if (z3::expr *z3_val = boost::get<z3::expr>(&result_tuple.second)) {
            result_vec->push_back(*z3_val);
        } else if (auto z3_var =
                       check_complex<ControlState>(result_tuple.second)) {
            for (auto sub_tuple : z3_var->state_vars) {
                result_vec->push_back(sub_tuple.second);
            }
        } else {
            BUG("Unsupported result type.");
        }
    }
}

int compare_progs(z3::context *ctx, std::vector<z3::expr> z3_repr_prog_before,
                  std::vector<z3::expr> z3_repr_prog_after) {
    z3::solver s(*ctx);
    for (size_t i = 0; i < z3_repr_prog_before.size(); ++i) {
        auto left_val = z3_repr_prog_before[i];
        auto right_val = z3_repr_prog_after[i];
        s.add(left_val == right_val);
    }
    switch (s.check()) {
    case z3::sat:
        printf("Programs are equal!\n");
        return EXIT_SUCCESS;
    case z3::unsat:
        printf("Programs are not equal! Found validation error.\n");
        return EXIT_FAILURE;
    case z3::unknown:
        printf("Could not determine equality. Error\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
} // namespace TOZ3_V2

int main(int argc, char *const argv[]) {
    setup_gc_logging();

    AutoCompileContext autoP4toZ3Context(new TOZ3_V2::P4toZ3Context);
    auto &options = TOZ3_V2::P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test";

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0) {
        return EXIT_FAILURE;
    }

    auto hook = options.getDebugHook();

    // check input and output file
    if (options.file == nullptr || options.compare_file == nullptr) {
        options.usage();
        return EXIT_FAILURE;
    }

    const IR::P4Program *prog_before = nullptr;
    const IR::P4Program *prog_after = nullptr;

    // parse the first program
    prog_before = P4::parseP4File(options);

    // use a little trick here to get the second program
    options.file = options.compare_file;
    prog_after = P4::parseP4File(options);

    z3::context ctx;

    TOZ3_V2::P4Z3Result z3_repr_prog_before =
        TOZ3_V2::get_z3_repr(prog_before, &ctx);
    std::vector<z3::expr> result_vec_before;
    TOZ3_V2::unroll_result(z3_repr_prog_before, &result_vec_before);
    TOZ3_V2::P4Z3Result z3_repr_prog_after =
        TOZ3_V2::get_z3_repr(prog_after, &ctx);
    std::vector<z3::expr> result_vec_after;
    TOZ3_V2::unroll_result(z3_repr_prog_after, &result_vec_after);

    int result =
        TOZ3_V2::compare_progs(&ctx, result_vec_before, result_vec_after);
    return result;
}
