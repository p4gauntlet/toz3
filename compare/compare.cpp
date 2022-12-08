#include "compare.h"

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>

#include <boost/format.hpp>

#include "frontends/common/parseInput.h"
#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/util.h"
#include "toz3/common/visitor_interpret.h"
#include "z3++.h"
#include "z3_api.h"

namespace TOZ3 {

// Passes that are not supported for translation validation.
static const std::array<cstring, 1> SKIPPED_PASSES = {"FlattenHeaderUnion"};

MainResult get_z3_repr(cstring prog_name, const IR::P4Program* program, z3::context* ctx) {
    try {
        // Convert the P4 program to Z3
        TOZ3::P4State state(ctx);
        TOZ3::Z3Visitor to_z3(&state, false);
        program->apply(to_z3);
        const auto* decl = get_main_decl(&state);
        if (decl == nullptr) {
            return {};
        }
        TOZ3::Z3Visitor to_z3_second(&state);
        return gen_state_from_instance(&to_z3_second, decl);
    } catch (const Util::P4CExceptionBase& bug) {
        std::cerr << "Failed to interpret pass \"" << prog_name << "\"." << std::endl;
        std::cerr << bug.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (z3::exception& ex) {
        std::cerr << "Failed to interpret pass \"" << prog_name << "\"." << std::endl;
        std::cerr << "Z3 exception: " << ex << std::endl;
        exit(EXIT_FAILURE);
    }
    return {};
}

void unroll_result(const MainResult& z3_repr_prog,
                   std::vector<std::pair<cstring, z3::expr>>* result_vec) {
    for (const auto& result_tuple : z3_repr_prog) {
        auto name = result_tuple.first;
        auto z3_result = result_tuple.second.first;
        for (const auto& sub_tuple : z3_result) {
            auto sub_name = name + "_" + sub_tuple.first;
            result_vec->push_back({sub_name, sub_tuple.second});
        }
    }
}

z3::expr create_z3_struct(z3::context* ctx,
                          const std::vector<std::pair<cstring, z3::expr>>& z3_prog) {
    z3::expr_vector z3_vec(*ctx);
    std::vector<z3::sort> z3_vec_sorts;
    std::vector<const char*> names;
    z3::func_decl_vector getters(*ctx);

    for (const auto& prog_tuple : z3_prog) {
        cstring before_name = "before" + prog_tuple.first;
        names.push_back(before_name.c_str());
        z3_vec.push_back(prog_tuple.second);
        z3_vec_sorts.push_back(prog_tuple.second.get_sort());
    }
    auto before_sort =
        ctx->tuple_sort("State", z3_vec.size(), names.data(), z3_vec_sorts.data(), getters);

    return before_sort(z3_vec);
}

void print_violation_error(const z3::solver& s, const Z3Prog& prog_before,
                           const Z3Prog& prog_after) {
    std::cerr << "Found validation error.\n";
    std::cerr << "Program " << prog_before.first << " before:\n";
    for (const auto& prog_tuple_before : prog_before.second) {
        cstring left_name = prog_tuple_before.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH) << prog_tuple_before.second.simplify()
                  << std::endl;
    }
    std::cerr << "\nProgram " << prog_after.first << " after:\n";
    for (const auto& prog_tuple_after : prog_after.second) {
        cstring left_name = prog_tuple_after.first + ": ";
        std::cerr << std::left << std::setw(COLUMN_WIDTH) << left_name;
        std::cerr << std::right << std::setw(COLUMN_WIDTH) << prog_tuple_after.second.simplify()
                  << std::endl;
    }
    auto model = s.get_model();
    std::cerr << "\nSolution :\n";
    for (size_t idx = 0; idx < model.size(); idx++) {
        auto var = model[idx];
        std::cerr << var.name() << " = " << model.get_const_interp(var) << std::endl;
    }
}

z3::expr substitute_taint(z3::context* ctx, const z3::expr& z3_var,
                          std::set<z3::expr>* taint_vars) {
    auto decl = z3_var.decl();
    auto z3_sort = z3_var.get_sort();
    if (decl.decl_kind() == Z3_OP_ITE) {
        auto cond_expr = z3_var.arg(0);
        auto then_expr = z3_var.arg(1);
        auto else_expr = z3_var.arg(2);
        std::set<z3::expr> cond_taint_vars;
        cond_expr = substitute_taint(ctx, cond_expr, &cond_taint_vars);
        // Check if the cond expr is an ite statement after substitution.
        // If the condition is tainted, do not even bother to evaluate the rest.
        if (cond_expr.decl().decl_kind() != Z3_OP_ITE && !cond_taint_vars.empty()) {
            auto taint_const = z3::expr(*ctx, Z3_mk_fresh_const(*ctx, "taint", z3_sort));
            taint_vars->insert(taint_const);
            return taint_const;
        }
        // Evaluate the branches.
        std::set<z3::expr> then_taint_vars;
        then_expr = substitute_taint(ctx, then_expr, &then_taint_vars);
        std::set<z3::expr> else_taint_vars;
        else_expr = substitute_taint(ctx, else_expr, &else_taint_vars);
        // Check if the branches are an ite statement after substitution.
        if (then_expr.decl().decl_kind() != Z3_OP_ITE &&
            else_expr.decl().decl_kind() != Z3_OP_ITE && !then_taint_vars.empty() &&
            !else_taint_vars.empty()) {
            // Both branches are fully tainted. Replace and return.
            auto taint_const = z3::expr(*ctx, Z3_mk_fresh_const(*ctx, "taint", z3_sort));
            taint_vars->insert(taint_const);
            return taint_const;
        }
        // Merge taints and create a new ite statement
        taint_vars->insert(cond_taint_vars.begin(), cond_taint_vars.end());
        taint_vars->insert(then_taint_vars.begin(), then_taint_vars.end());
        taint_vars->insert(else_taint_vars.begin(), else_taint_vars.end());
        if (!taint_vars->empty()) {
            return z3::ite(cond_expr, then_expr, else_expr);
        }
        return z3_var;
    }
    if (z3_var.is_const() && z3_var.to_string().find("undefined") != std::string::npos) {
        // The expression is tainted replace it.
        // Really dumb check, do not need anything else.
        auto taint_const = z3::expr(*ctx, Z3_mk_fresh_const(*ctx, "taint", z3_sort));
        taint_vars->insert(taint_const);
        return taint_const;
    }
    // Remaining expressions are more complex, need to evaluate children.
    auto arg_num = z3_var.num_args();
    z3::expr_vector new_child_vars(*ctx);
    for (size_t idx = 0; idx < arg_num; ++idx) {
        auto child = z3_var.arg(idx);
        std::set<z3::expr> child_taint_vars;
        child = substitute_taint(ctx, child, &child_taint_vars);
        // Replace entire expression if one non-ite member is tainted.
        if (child.decl().decl_kind() != Z3_OP_ITE && !child_taint_vars.empty()) {
            // The expression is tained. Replace it.
            auto taint_const = z3::expr(*ctx, Z3_mk_fresh_const(*ctx, "taint", z3_sort));
            taint_vars->insert(taint_const);
            return taint_const;
        }
        taint_vars->insert(child_taint_vars.begin(), child_taint_vars.end());
        new_child_vars.push_back(child);
    }
    // We have taint, return a new expression.
    if (!taint_vars->empty()) {
        return decl(new_child_vars);
    }
    return z3_var;
}

z3::check_result check_undefined(z3::context* ctx, z3::solver* s, const z3::expr& z3_prog_before,
                                 const z3::expr& z3_prog_after) {
    auto arg_num = z3_prog_before.num_args();
    s->reset();
    for (size_t idx = 0; idx < arg_num; ++idx) {
        s->push();
        auto m_before = z3_prog_before.arg(idx).simplify();
        auto m_after = z3_prog_after.arg(idx).simplify();
        std::set<z3::expr> taint_vars;
        m_before = substitute_taint(ctx, m_before, &taint_vars);
        z3::expr tv_equiv = (m_before != m_after);
        for (const auto& taint_var : taint_vars) {
            if (m_before.get_sort().sort_kind() == taint_var.get_sort().sort_kind()) {
                tv_equiv = tv_equiv && m_before != taint_var;
            }
        }
        // Check the equivalence of the modified clause.
        Logger::log_msg(1, "Checking member %s... ", idx);
        cstring equ = tv_equiv.to_string().c_str();
        Logger::log_msg(1, "Equation:\n%s", equ);
        s->add(tv_equiv);
        auto ret = s->check();
        s->pop();
        if (ret != z3::unsat) {
            std::cerr << "Violation holds despite undefined behavior check.";
            return ret;
        }
    }
    std::cerr << "Violation was caused by undefined behavior." << std::endl;
    return z3::check_result::unsat;
}

int compare_progs(z3::context* ctx, const std::vector<Z3Prog>& z3_progs, bool allow_undefined) {
    z3::solver s(*ctx);
    auto prog_before = z3_progs[0];
    auto z3_prog_before = create_z3_struct(ctx, prog_before.second);
    for (size_t i = 1; i < z3_progs.size(); ++i) {
        auto prog_after = z3_progs[i];
        auto z3_prog_after = create_z3_struct(ctx, z3_progs[i].second);

        bool found = false;
        for (auto banned_pass : SKIPPED_PASSES) {
            if (prog_before.first.find(banned_pass.c_str()) != nullptr ||
                prog_after.first.find(banned_pass.c_str()) != nullptr) {
                found = true;
                break;
            }
        }
        if (found) {
            prog_before = prog_after;
            z3_prog_before = z3_prog_after;
            continue;
        }
        Logger::log_msg(1, "\nComparing %s and %s.", prog_before.first, prog_after.first);

        s.push();
        s.add(z3_prog_before != z3_prog_after);
        Logger::log_msg(1, "Checking... ");
        auto ret = s.check();
        Logger::log_msg(1, "Result: %s", ret);
        if (ret == z3::sat) {
            s.pop();
            std::cerr << "Programs are not equal!" << std::endl;
            if (allow_undefined) {
                std::cerr << "Rechecking whether violation is caused by "
                             "undefined behavior."
                          << std::endl;
                ret = check_undefined(ctx, &s, z3_prog_before, z3_prog_after);
                if (ret != z3::unsat) {
                    print_violation_error(s, prog_before, prog_after);
                    return EXIT_VIOLATION;
                }
                prog_before = prog_after;
                z3_prog_before = z3_prog_after;
                continue;
            }
            print_violation_error(s, prog_before, prog_after);
            return EXIT_VIOLATION;
        }
        if (ret == z3::unknown) {
            std::cerr << "Error: Could not determine equality. Error" << std::endl;
            return EXIT_FAILURE;
        }
        s.pop();
        prog_before = prog_after;
        z3_prog_before = z3_prog_after;
    }
    Logger::log_msg(0, "Passed all checks.");
    return EXIT_SUCCESS;
}

int process_programs(const std::vector<cstring>& prog_list, ParserOptions* options,
                     bool allow_undefined) {
    z3::context ctx;
    // Parse the first program
    // Use a little trick here to get the second program
    std::vector<Z3Prog> z3_progs;
    for (auto prog : prog_list) {
        options->file = prog;
        const auto* prog_parsed = P4::parseP4File(*options);
        if (prog_parsed == nullptr || ::errorCount() > 0) {
            std::cerr << "Unable to parse program." << std::endl;
            return EXIT_FAILURE;
        }
        auto z3_repr_prog = get_z3_repr(prog, prog_parsed, &ctx);
        std::vector<std::pair<cstring, z3::expr>> result_vec;
        unroll_result(z3_repr_prog, &result_vec);
        z3_progs.emplace_back(prog, result_vec);
    }
    return compare_progs(&ctx, z3_progs, allow_undefined);
}

}  // namespace TOZ3
