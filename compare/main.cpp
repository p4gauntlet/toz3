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

#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/log.h"
#include "lib/nullstream.h"

#include "options.h"
#include "toz3_v2/common/visitor_fill_type.h"
#include "toz3_v2/common/visitor_interpret.h"

namespace TOZ3_V2 {

#define EXIT_SKIPPED 10
#define EXIT_VALIDATION 20
#define EXIT_UNDEFINED 30

const IR::Declaration_Instance *get_main_decl(TOZ3_V2::P4State *state) {
    auto main = state->get_static_decl("main");
    if (auto main_pkg = main->decl->to<IR::Declaration_Instance>()) {
        return main_pkg;
    } else {
        P4C_UNIMPLEMENTED("Main node %s not implemented!",
                          main->decl->node_type_name());
    }
}

VarMap get_z3_repr(const IR::P4Program *program, z3::context *ctx) {
    VarMap z3_return;

    if (program != nullptr && ::errorCount() == 0) {
        try {
            // convert the P4 program to Z3
            P4State state(ctx);
            TypeVisitor map_builder = TypeVisitor(&state);
            Z3Visitor to_z3 = Z3Visitor(&state);
            program->apply(map_builder);
            auto decl = get_main_decl(&state);
            decl->apply(to_z3);
            auto decl_result = to_z3.get_main_result();
            return decl_result;
        } catch (const Util::P4CExceptionBase &bug) {
            std::cerr << bug.what() << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    return z3_return;
}

void unroll_result(VarMap z3_repr_prog, std::vector<z3::expr> *result_vec) {
    for (auto result_tuple : z3_repr_prog) {
        if (auto z3_val = result_tuple.second.first->to<Z3Bitvector>()) {
            result_vec->push_back(z3_val->val);
        } else if (auto z3_var =
                       result_tuple.second.first->to<ControlState>()) {
            for (auto sub_tuple : z3_var->state_vars) {
                result_vec->push_back(sub_tuple.second);
            }
        } else {
            P4C_UNIMPLEMENTED("Unsupported result type.");
        }
    }
}

int compare_progs(
    z3::context *ctx,
    std::vector<std::pair<cstring, std::vector<z3::expr>>> z3_progs) {
    z3::solver s(*ctx);

    for (size_t i = 1; i < z3_progs.size(); ++i) {
        s.push();
        cstring prog_before_name = z3_progs[i - 1].first;
        cstring prog_after_name = z3_progs[i].first;
        printf("Comparing %s and %s \n", prog_before_name.c_str(),
               prog_after_name.c_str());
        std::vector<z3::expr> z3_repr_prog_before = z3_progs[i - 1].second;
        std::vector<z3::expr> z3_repr_prog_after = z3_progs[i].second;
        z3::expr_vector z3_vec_before(*ctx);
        z3::expr_vector z3_vec_after(*ctx);
        std::vector<z3::sort> z3_vec_before_sorts;
        std::vector<z3::sort> z3_vec_after_sorts;
        std::vector<const char *> before_names;
        std::vector<const char *> after_names;
        z3::func_decl_vector before_getters(*ctx);
        z3::func_decl_vector after_getters(*ctx);

        for (size_t i = 0; i < z3_repr_prog_before.size(); ++i) {
            cstring before_name = "before" + std::to_string(i);
            cstring after_name = "after" + std::to_string(i);
            before_names.push_back(before_name.c_str());
            after_names.push_back(after_name.c_str());
            z3_vec_before.push_back(z3_repr_prog_before[i]);
            z3_vec_after.push_back(z3_repr_prog_after[i]);
            z3_vec_before_sorts.push_back(z3_repr_prog_after[i].get_sort());
            z3_vec_after_sorts.push_back(z3_repr_prog_after[i].get_sort());
        }
        z3::func_decl before_sort = ctx->tuple_sort(
            "State_before", z3_vec_before.size(), before_names.data(),
            z3_vec_before_sorts.data(), before_getters);

        z3::func_decl after_sort = ctx->tuple_sort(
            "State_before", z3_vec_after.size(), after_names.data(),
            z3_vec_after_sorts.data(), after_getters);
        z3::expr prog_before = before_sort(z3_vec_before);
        z3::expr prog_after = after_sort(z3_vec_after);

        s.add(prog_before != prog_after);
        std::cout << "Checking... " << std::endl;
        auto ret = s.check();
        std::cout << "Result: " << ret << std::endl;
        if (ret == z3::sat) {
            error("Programs are not equal! Found validation error.\n");
            error("Program %s before %s", prog_before_name,
                  prog_before.simplify().to_string().c_str());
            error("Program %s after %s", prog_after_name,
                  prog_after.simplify().to_string().c_str());
            return EXIT_VALIDATION;
        } else if (ret == z3::unknown) {
            error("Could not determine equality. Error\n");
            return EXIT_FAILURE;
        }
        s.pop();
    }
    printf("Passed all checks.\n");
    return EXIT_SUCCESS;
}
} // namespace TOZ3_V2

std::vector<cstring> split_input_progs(cstring input_progs) {
    std::vector<cstring> prog_list;
    const char *pos = 0;
    cstring prog;

    while ((pos = input_progs.find((size_t)',')) != nullptr) {
        size_t idx = (size_t)(pos - input_progs);
        prog = input_progs.substr(0, idx);
        prog_list.push_back(prog);
        input_progs = input_progs.substr(idx + 1);
    }
    prog_list.push_back(input_progs);
    return prog_list;
}

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

    // check input file
    if (options.file == nullptr) {
        options.usage();
        return EXIT_FAILURE;
    }
    auto prog_list = split_input_progs(options.file);
    if (prog_list.size() < 2) {
        error("At least two input programs expected.");
        options.usage();
        return EXIT_FAILURE;
    }

    z3::context ctx;
    const IR::P4Program *prog_parsed = nullptr;
    // parse the first program
    // use a little trick here to get the second program
    std::vector<std::pair<cstring, std::vector<z3::expr>>> z3_progs;
    for (auto prog : prog_list) {
        options.file = prog;
        prog_parsed = P4::parseP4File(options);
        auto z3_repr_prog = TOZ3_V2::get_z3_repr(prog_parsed, &ctx);
        std::vector<z3::expr> result_vec;
        TOZ3_V2::unroll_result(z3_repr_prog, &result_vec);
        z3_progs.push_back({prog, result_vec});
    }
    return TOZ3_V2::compare_progs(&ctx, z3_progs);
    ;
}
