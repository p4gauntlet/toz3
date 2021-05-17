
#include <cstdio>
#include <iostream>
#include <utility>

#include "../contrib/z3/z3++.h"

#include "visitor_interpret.h"

namespace TOZ3 {

// void unroll_list(const StructBase *input_struct,
//                  std::vector<P4Z3Instance *> *target_list) {
//     for (const auto &member : *input_struct->get_member_map()) {
//         auto *member_instance = member.second;
//         if (const auto *sb = member_instance->to<StructBase>()) {
//             unroll_list(sb, target_list);
//         } else {
//             target_list->push_back(member_instance);
//         }
//     }
// }

bool Z3Visitor::preorder(const IR::SelectExpression *se) {
    BUG_CHECK(!se->selectCases.empty(), "Case vector can not be empty.");
    visit(se->select);
    const auto *select_cond = state->copy_expr_result<ListInstance>();
    // First gather the right conditions
    z3::expr matches = state->get_z3_ctx()->bool_val(false);
    std::vector<std::pair<z3::expr, const IR::PathExpression *>> select_vector;
    for (const auto *select_case : se->selectCases) {
        if (select_case->keyset->is<IR::DefaultExpression>()) {
            select_vector.emplace_back(!matches, select_case->state);
        } else {
            visit(select_case->keyset);
            auto cond = *select_cond == *state->get_expr_result();
            select_vector.emplace_back(cond, select_case->state);
            matches = matches || cond;
        }
    }
    // Now evaluate all the select cases
    bool has_exited = true;
    bool has_returned = true;
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    for (auto &select : select_vector) {
        const auto cond = select.first;
        const auto *se_state = select.second;
        auto old_vars = state->clone_vars();
        state->push_forward_cond(cond);
        auto path_name = se_state->path->name.name;
        const auto *decl = state->get_static_decl(path_name);
        if (!state_is_visited(path_name)) {
            visit(decl->decl);
        }

        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (!(call_has_exited || stmt_has_returned)) {
            case_states.emplace_back(cond, state->get_vars());
        }
        has_exited = has_exited && call_has_exited;
        has_returned = has_returned && stmt_has_returned;
        state->set_exit(false);
        state->set_returned(false);
        state->restore_vars(old_vars);
    }
    state->set_exit(has_exited);
    state->set_returned(has_returned);
    for (auto it = case_states.rbegin(); it != case_states.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::ParserState *ps) {
    auto state_name = ps->name.name;
    add_visited_state(state_name);
    for (const auto *component : ps->components) {
        visit(component);
    }
    if (const auto *path = ps->selectExpression->to<IR::PathExpression>()) {
        auto path_name = path->path->name.name;
        const auto *decl = state->get_static_decl(path_name);
        if (!state_is_visited(path_name)) {
            visit(decl->decl);
        }
    } else if (const auto *se =
                   ps->selectExpression->to<IR::SelectExpression>()) {
        visit(se);
    } else {
        P4C_UNIMPLEMENTED("SelectExpression of type %s not implemented.",
                          ps->selectExpression->node_type_name());
    }
    return false;
}

}  // namespace TOZ3
