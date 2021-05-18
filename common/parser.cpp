
#include <cstdio>
#include <iostream>
#include <utility>

#include "../contrib/z3/z3++.h"

#include "visitor_interpret.h"

namespace TOZ3 {

void unroll_list(const StructBase *input_struct,
                 std::vector<P4Z3Instance *> *target_list) {
    for (const auto &member : *input_struct->get_member_map()) {
        auto *member_instance = member.second;
        if (const auto *sb = member_instance->to<StructBase>()) {
            unroll_list(sb, target_list);
        } else {
            target_list->push_back(member_instance);
        }
    }
}

std::vector<P4Z3Instance *> get_vec_from_map(const StructBase *input_struct) {
    std::vector<P4Z3Instance *> target_list;
    for (const auto &member : *input_struct->get_member_map()) {
        auto *member_instance = member.second;
        target_list.push_back(member_instance);
    }
    return target_list;
}

z3::expr handle_select_cond(Z3Visitor *visitor, const StructBase *select_list,
                            const IR::ListExpression *list_expr);

z3::expr check_cond(Z3Visitor *visitor, const P4Z3Instance *select_eval,
                    const IR::Expression *match_key) {
    auto *state = visitor->state;
    if (const auto *range = match_key->to<IR::Range>()) {
        // TODO: A hack to deal with mismatch between lists and ranges
        if (const auto *li = select_eval->to<ListInstance>()) {
            select_eval = li->get_member_map()->begin()->second;
        }
        visitor->visit(range->left);
        const auto *min = state->copy_expr_result();
        visitor->visit(range->right);
        const auto *max = state->get_expr_result();
        return *min <= *select_eval && *select_eval <= *max;
    }
    if (const auto *mask_expr = match_key->to<IR::Mask>()) {
        // TODO: A hack to deal with mismatch between lists and masks
        if (const auto *li = select_eval->to<ListInstance>()) {
            select_eval = li->get_member_map()->begin()->second;
        }
        visitor->visit(mask_expr->left);
        const auto *val = state->copy_expr_result();
        visitor->visit(mask_expr->right);
        const auto *mask = state->get_expr_result();
        // TODO: Check_eq is a hack. Replace
        return check_eq(*select_eval & *mask, *val & *mask);
    }
    if (const auto *match_list_expr = match_key->to<IR::ListExpression>()) {
        const auto *struct_select_eval = select_eval->to<StructBase>();
        return handle_select_cond(visitor, struct_select_eval, match_list_expr);
    }
    if (match_key->is<IR::DefaultExpression>()) {
        return state->get_z3_ctx()->bool_val(true);
    }
    visitor->visit(match_key);
    return *select_eval == *state->get_expr_result();
}

z3::expr handle_select_cond(Z3Visitor *visitor, const StructBase *select_list,
                            const IR::ListExpression *list_expr) {
    auto *state = visitor->state;
    z3::expr match_cond = state->get_z3_ctx()->bool_val(true);
    const auto select_members = get_vec_from_map(select_list);

    for (size_t idx = 0; idx < list_expr->size(); ++idx) {
        const auto *match_key = list_expr->components.at(idx);
        // TODO: tostringy no likey
        const auto *select_eval = select_members.at(idx);
        match_cond = match_cond && check_cond(visitor, select_eval, match_key);
    }
    return match_cond;
}

bool Z3Visitor::preorder(const IR::SelectExpression *se) {
    // First gather the right conditions
    z3::expr matches = state->get_z3_ctx()->bool_val(false);
    std::vector<std::pair<z3::expr, const IR::PathExpression *>> select_vector;
    for (const auto *select_case : se->selectCases) {
        if (select_case->keyset->is<IR::DefaultExpression>()) {
            select_vector.emplace_back(!matches, select_case->state);
        } else {
            BUG_CHECK(!se->selectCases.empty(),
                      "Case vector can not be empty.");
            visit(se->select);
            const auto *list_instance = state->copy_expr_result<ListInstance>();
            if (const auto *list_expr =
                    select_case->keyset->to<IR::ListExpression>()) {
                auto cond = handle_select_cond(this, list_instance, list_expr);
                select_vector.emplace_back(cond, select_case->state);
                matches = matches || cond;
            } else {
                auto cond =
                    check_cond(this, list_instance, select_case->keyset);
                select_vector.emplace_back(cond, select_case->state);
                matches = matches || cond;
            }
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
        auto old_visited_states = get_visited_states();
        if (!state_is_visited(path_name)) {
            visit(decl->decl);
        }
        set_visited_states(old_visited_states);
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
    // If there is no select expression we automatically transition to
    // reject
    if (ps->selectExpression == nullptr) {
        visit(state->get_static_decl("reject")->decl);
        return false;
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
