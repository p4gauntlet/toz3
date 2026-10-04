#include "compare.h"

#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

#include "frontends/common/parseInput.h"
#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/exceptions.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/util.h"
#include "toz3/common/visitor_interpret.h"
#include "z3++.h"
#include "z3_api.h"

namespace P4::ToZ3 {

bool is_undefined_const(const z3::expr &e) {
    if (!e.is_const()) {
        return false;
    }
    // Check if the constant's name starts with the UNDEF_LABEL prefix.
    // Using rfind with a 0 position is a reliable way to check for a prefix.
    return std::string(e.decl().name().str()).rfind(UNDEF_LABEL, 0) == 0;
}

MainResult get_z3_repr(const std::filesystem::path &prog_name, const IR::P4Program *program,
                       z3::context *ctx, z3::expr *input_constraints) {
    try {
        // Convert the P4 program to Z3
        P4State state(ctx);
        Z3Visitor to_z3(&state, false);
        program->apply(to_z3);
        const auto *decl = get_main_decl(&state);
        if (decl == nullptr) {
            return {};
        }
        Z3Visitor to_z3_second(&state);
        auto result = gen_state_from_instance(&to_z3_second, decl);
        *input_constraints = state.get_input_constraints().simplify();
        return result;
    } catch (const Util::P4CExceptionBase &bug) {
        throw P4::ToZ3::InternalError("Failed to interpret pass \"" + prog_name.string() + "\".\n" +
                                      bug.what());
    } catch (z3::exception &ex) {
        throw P4::ToZ3::Z3Error("Failed to interpret pass \"" + prog_name.string() +
                                "\".\nZ3 exception: " + ex.msg());
    }
}

void unroll_result(const MainResult &z3_repr_prog,
                   std::vector<std::pair<cstring, z3::expr>> *result_vec) {
    for (const auto &result_tuple : z3_repr_prog) {
        auto name = result_tuple.first;
        const auto &z3_result = result_tuple.second.first;
        for (const auto &sub_tuple : z3_result) {
            auto sub_name = name + "_" + sub_tuple.first;
            result_vec->push_back({sub_name, sub_tuple.second});
        }
    }
}

z3::expr create_z3_struct(z3::context *ctx,
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
    auto before_sort =
        ctx->tuple_sort("State", z3_vec.size(), names.data(), z3_vec_sorts.data(), getters);

    // Rewrite the complete summary once so Z3 can share work across output fields.
    z3::params normalization(*ctx);
    // P4C strength reduction replaces multiplication by powers of two with shifts.
    // Canonicalize both forms before they become arguments of packet-read functions.
    normalization.set("mul2concat", true);
    return before_sort(z3_vec).simplify(normalization);
}

void print_violation_error(const z3::solver &s, const Z3Prog &prog_before,
                           const Z3Prog &prog_after) {
    std::cerr << "Found validation error.\n";
    std::cerr << "Program " << prog_before.name << " before:\n";
    for (const auto &prog_tuple_before : prog_before.outputs) {
        cstring left_name = prog_tuple_before.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH) << prog_tuple_before.second.simplify()
                  << std::endl;
    }
    std::cerr << "\nProgram " << prog_after.name << " after:\n";
    for (const auto &prog_tuple_after : prog_after.outputs) {
        cstring left_name = prog_tuple_after.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH) << prog_tuple_after.second.simplify()
                  << std::endl;
    }
    auto model = s.get_model();
    std::cerr << "\nSolution :\n";
    std::cerr << model << std::endl;
}

namespace {

class Definedness {
    // Retain the ASTs so Z3 cannot recycle an ID when a field's simplified root dies.
    std::unordered_map<unsigned, std::pair<z3::expr, z3::expr>> conditions;

    static z3::expr both(const z3::expr &a, const z3::expr &b) {
        if (a.is_false() || b.is_true()) return a;
        if (b.is_false() || a.is_true() || z3::eq(a, b)) return b;
        return a && b;
    }

 public:
    z3::expr when(const z3::expr &expression) {
        // Traverse the shared DAG once, rather than expanding it as an expression tree.
        // Keep the traversal iterative because parser summaries can be deeply nested.
        std::vector<std::pair<z3::expr, bool>> pending{{expression, false}};
        while (!pending.empty()) {
            auto [node, expanded] = pending.back();
            pending.pop_back();
            if (conditions.count(node.id())) continue;
            if (node.is_const()) {
                conditions.emplace(
                    node.id(),
                    std::make_pair(node, node.ctx().bool_val(!is_undefined_const(node))));
            } else if (!expanded) {
                pending.emplace_back(node, true);
                for (unsigned i = 0; i < node.num_args(); ++i)
                    if (!conditions.count(node.arg(i).id()))
                        pending.emplace_back(node.arg(i), false);
            } else {
                auto defined = node.ctx().bool_val(true);
                if (node.is_ite()) {
                    const auto &yes = conditions.at(node.arg(1).id()).second;
                    const auto &no = conditions.at(node.arg(2).id()).second;
                    // Only the selected branch must be defined. In particular,
                    // an invalid-header read must not taint the valid-input path.
                    defined = both(conditions.at(node.arg(0).id()).second,
                                   z3::eq(yes, no) ? yes : z3::ite(node.arg(0), yes, no));
                } else {
                    for (unsigned i = 0; i < node.num_args(); ++i)
                        defined = both(defined, conditions.at(node.arg(i).id()).second);
                    if (!defined.is_true() && (node.is_and() || node.is_or())) {
                        // Simplification may turn a guarded undefined read into
                        // an AND/OR. A defined false/true operand determines the
                        // result even when another operand is undefined.
                        for (unsigned i = 0; i < node.num_args(); ++i) {
                            const auto &arg = node.arg(i);
                            const auto decisive =
                                both(conditions.at(arg.id()).second, node.is_and() ? !arg : arg);
                            if (decisive.is_false()) continue;
                            defined = defined.is_false() || decisive.is_true()
                                          ? decisive
                                          : defined || decisive;
                            if (defined.is_true()) break;
                        }
                    }
                }
                conditions.emplace(node.id(), std::make_pair(node, defined));
            }
        }
        return conditions.at(expression.id()).second;
    }
};

}  // namespace

z3::expr defined_when(const z3::expr &expression) {
    Definedness analysis;
    return analysis.when(expression);
}

z3::check_result check_undefined(z3::solver *s, const z3::expr &z3_prog_before,
                                 const z3::expr &z3_prog_after, const z3::expr &input_constraints) {
    auto arg_num = z3_prog_before.num_args();
    Definedness analysis;
    s->reset();
    s->add(input_constraints);
    for (size_t idx = 0; idx < arg_num; ++idx) {
        s->push();
        auto m_before = z3_prog_before.arg(idx).simplify();
        auto m_after = z3_prog_after.arg(idx).simplify();
        const auto tv_equiv = analysis.when(m_before) && m_before != m_after;
        // Check the equivalence of the modified clause.
        Logger::log_msg(1, "Checking member %s... ", idx);
        if (Logger::enabled(1)) {
            cstring equ = tv_equiv.to_string();
            Logger::log_msg(1, "Equation:\n%s", equ);
        }
        s->add(tv_equiv);
        auto ret = s->check();
        if (ret == z3::sat) {
            // Keep the failing assertion in scope so diagnostics can retrieve its model.
            std::cerr << "Violation holds despite undefined behavior check.";
            return ret;
        }
        s->pop();
        if (ret == z3::unknown) {
            throw Z3Error("Could not determine equality during undefined behavior check");
        }
    }
    std::cerr << "Violation was caused by undefined behavior." << std::endl;
    return z3::check_result::unsat;
}

int compareProgs(z3::context *ctx, const std::vector<std::filesystem::path> &prog_list,
                 const std::function<Z3Prog(const std::filesystem::path &)> &interpret,
                 bool allow_undefined, bool diagnostics) {
    z3::solver s(*ctx);
    auto prog_before = interpret(prog_list.front());
    auto input_constraints = prog_before.input_constraints;
    auto z3_prog_before = create_z3_struct(ctx, prog_before.outputs);
    for (size_t i = 1; i < prog_list.size(); ++i) {
        // Retain only adjacent summaries, and stop interpreting at the first violation.
        auto prog_after = interpret(prog_list[i]);
        auto z3_prog_after = create_z3_struct(ctx, prog_after.outputs);
        // Preserve the source input domain after a lowering removes union types.
        input_constraints = (input_constraints && prog_before.input_constraints).simplify();
        Logger::log_msg(1, "\nComparing %s and %s.", prog_before.name, prog_after.name);
        // Each adjacent pair is an independent query. Keep a failing query
        // intact so diagnostics can retrieve its counterexample model.
        s.reset();
        s.add(input_constraints);
        s.add(z3_prog_before != z3_prog_after);
        Logger::log_msg(1, "Checking... ");
        auto ret = s.check();
        Logger::log_msg(1, "Result: %s", ret);
        if (ret == z3::sat && allow_undefined) {
            std::cerr << "Rechecking whether violation is caused by "
                         "undefined behavior."
                      << std::endl;
            ret = check_undefined(&s, z3_prog_before, z3_prog_after, input_constraints);
        }
        if (ret == z3::sat) {
            if (diagnostics) print_violation_error(s, prog_before, prog_after);
            return EXIT_VIOLATION;
        }
        if (ret == z3::unknown) {
            throw Z3Error("Could not determine equality between " + prog_before.name.string() +
                          " and " + prog_after.name.string());
        }
        prog_before = std::move(prog_after);
        z3_prog_before = z3_prog_after;
    }
    Logger::log_msg(0, "Passed all checks.");
    return EXIT_SUCCESS;
}

int process_programs(const std::vector<std::filesystem::path> &prog_list, ParserOptions *options,
                     bool allow_undefined, bool diagnostics) {
    if (prog_list.empty()) return EXIT_SKIPPED;
    z3::context ctx;
    const auto interpret = [&](const std::filesystem::path &prog) -> Z3Prog {
        options->file = prog;
        const auto *progParsed = P4::parseP4File(*options);
        if (progParsed == nullptr || P4::errorCount() > 0) {
            throw GauntletException("Unable to parse program: " + prog.string());
        }
        auto input_constraints = ctx.bool_val(true);
        auto z3ReprProg = get_z3_repr(prog, progParsed, &ctx, &input_constraints);
        std::vector<std::pair<cstring, z3::expr>> resultVec;
        unroll_result(z3ReprProg, &resultVec);
        return Z3Prog{cstring(prog.string()), std::move(resultVec), input_constraints};
    };
    return compareProgs(&ctx, prog_list, interpret, allow_undefined, diagnostics);
}

}  // namespace P4::ToZ3
