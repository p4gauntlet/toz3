#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <ostream>

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

using Z3Prog = std::pair<cstring, std::vector<std::pair<cstring, z3::expr>>>;
constexpr auto COLUMN_WIDTH = 40;

namespace TOZ3 {

const IR::Declaration_Instance *get_main_decl(P4State *state) {
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

VarMap get_z3_repr(cstring prog_name, const IR::P4Program *program,
                   z3::context *ctx) {
    VarMap z3_return;

    try {
        // convert the P4 program to Z3
        P4State state(ctx);
        TypeVisitor map_builder = TypeVisitor(&state);
        program->apply(map_builder);
        Z3Visitor to_z3 = Z3Visitor(&state);
        const auto *decl = get_main_decl(&state);
        if (decl == nullptr) {
            return z3_return;
        }
        return to_z3.gen_state_from_instance(decl);
    } catch (const Util::P4CExceptionBase &bug) {
        std::cerr << "Failed to interpret pass \"" << prog_name << "\"."
                  << std::endl;
        std::cerr << bug.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (z3::exception &ex) {
        std::cerr << "Failed to interpret pass \"" << prog_name << "\"."
                  << std::endl;
        std::cerr << "Z3 exception: " << ex << std::endl;
        exit(EXIT_FAILURE);
    }
    return z3_return;
}

void unroll_result(const VarMap &z3_repr_prog,
                   std::vector<std::pair<cstring, z3::expr>> *result_vec) {
    for (const auto &result_tuple : z3_repr_prog) {
        auto name = result_tuple.first;
        auto z3_result = result_tuple.second;
        if (const auto *num_var = z3_result.first->to<NumericVal>()) {
            result_vec->push_back({name, *num_var->get_val()});
        } else if (const auto *ctrl_var = z3_result.first->to<ControlState>()) {
            for (const auto &sub_tuple : ctrl_var->state_vars) {
                auto sub_name = name + "_" + sub_tuple.first;
                result_vec->push_back({sub_name, sub_tuple.second});
            }
        } else {
            P4C_UNIMPLEMENTED("Unsupported result type.");
        }
    }
}

z3::expr
create_z3_struct(z3::context *ctx,
                 const std::vector<std::pair<cstring, z3::expr>> &z3_prog) {
    z3::expr_vector z3_vec(*ctx);
    std::vector<z3::sort> z3_vec_sorts;
    std::vector<const char *> names;
    z3::func_decl_vector getters(*ctx);

    for (const auto &prog_tuple : z3_prog) {
        cstring before_name = "before" + prog_tuple.first;
        names.push_back(before_name.c_str());
        z3_vec.push_back(prog_tuple.second);
        z3_vec_sorts.push_back(prog_tuple.second.get_sort());
    }
    auto before_sort = ctx->tuple_sort("State", z3_vec.size(), names.data(),
                                       z3_vec_sorts.data(), getters);

    return before_sort(z3_vec);
}

void print_violation_error(const z3::solver &s, const Z3Prog &prog_before,
                           const Z3Prog &prog_after) {
    std::cerr << "\n\nPrograms are not equal! Found validation error.\n";
    std::cerr << "Program " << prog_before.first << " before:\n";
    for (const auto &prog_tuple_before : prog_before.second) {
        cstring left_name = prog_tuple_before.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH)
                  << prog_tuple_before.second.simplify() << std::endl;
    }
    std::cerr << "\nProgram " << prog_after.first << " after:\n";
    for (const auto &prog_tuple_after : prog_after.second) {
        cstring left_name = prog_tuple_after.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH)
                  << prog_tuple_after.second.simplify() << std::endl;
    }
    auto model = s.get_model();
    std::cerr << "\nSolution :\n";
    for (size_t idx = 0; idx < model.size(); idx++) {
        auto var = model[idx];
        std::cerr << var.name() << " = " << model.get_const_interp(var)
                  << std::endl;
    }
}

int compare_progs(z3::context *ctx, const std::vector<Z3Prog> &z3_progs) {
    z3::solver s(*ctx);
    auto prog_before = z3_progs[0];
    auto z3_prog_before = create_z3_struct(ctx, prog_before.second);
    for (size_t i = 1; i < z3_progs.size(); ++i) {
        auto prog_after_name = z3_progs[i].first;
        std::cout << "Comparing " << prog_before.first << " and "
                  << prog_after_name << "." << std::endl;
        auto prog_after = z3_progs[i];
        auto z3_prog_after = create_z3_struct(ctx, z3_progs[i].second);

        s.push();
        s.add(z3_prog_before != z3_prog_after);
        std::cout << "Checking... " << std::endl;
        auto ret = s.check();
        std::cout << "Result: " << ret << std::endl;
        if (ret == z3::sat) {
            print_violation_error(s, prog_before, prog_after);
            return EXIT_VIOLATION;
        }
        if (ret == z3::unknown) {
            std::cerr << "Error: Could not determine equality. Error"
                      << std::endl;
            return EXIT_FAILURE;
        }
        s.pop();
        prog_before = prog_after;
    }
    std::cout << "Passed all checks." << std::endl;
    return EXIT_SUCCESS;
}
}  // namespace TOZ3

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
        std::cerr << "At least two input programs expected." << std::endl;
        options.usage();
        return EXIT_FAILURE;
    }

    z3::context ctx;
    // Parse the first program
    // Use a little trick here to get the second program
    std::vector<Z3Prog> z3_progs;
    for (auto prog : prog_list) {
        options.file = prog;
        const auto *prog_parsed = P4::parseP4File(options);
        if (prog_parsed != nullptr && ::errorCount() == 0) {
            P4::P4COptionPragmaParser optionsPragmaParser;
            prog_parsed->apply(P4::ApplyOptionsPragmas(optionsPragmaParser));
        } else {
            std::cerr << "Unable to parse program." << std::endl;
            return EXIT_FAILURE;
        }
        auto z3_repr_prog = TOZ3::get_z3_repr(prog, prog_parsed, &ctx);
        std::vector<std::pair<cstring, z3::expr>> result_vec;
        TOZ3::unroll_result(z3_repr_prog, &result_vec);
        z3_progs.emplace_back(prog, result_vec);
    }
    return TOZ3::compare_progs(&ctx, z3_progs);
}
