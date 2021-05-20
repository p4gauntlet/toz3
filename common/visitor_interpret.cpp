#include <cstdio>
#include <utility>

#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/log.h"
#include "type_complex.h"
#include "type_simple.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"
#include "visitor_specialize.h"

namespace TOZ3 {

bool Z3Visitor::preorder(const IR::P4Action *a) {
    visit(a->body);
    state->set_expr_result(new VoidResult());
    return false;
}

bool Z3Visitor::preorder(const IR::Function *f) {
    visit(f->body);

    // We start with the last return expression, which is the final return.
    // The final return may not have a condition, so this is a good fit.
    auto return_exprs = state->get_return_exprs();
    auto begin = return_exprs.rbegin();
    auto end = return_exprs.rend();
    if (begin != end) {
        const auto *return_type = f->type->returnType;
        auto *merged_return = begin->second->cast_allocate(return_type);
        for (auto it = std::next(begin); it != end; ++it) {
            z3::expr cond = it->first;
            const auto *then_var = it->second->cast_allocate(return_type);
            merged_return->merge(cond, *then_var);
        }
        state->set_expr_result(merged_return);
    } else {
        // if there are no return expression return a void result
        state->set_expr_result(new VoidResult());
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Method *m) {
    auto method_name = infer_name(m->getAnnotations(), m->name.name);
    const auto *method_type = state->resolve_type(m->type->returnType);
    // TODO: Different types of arguments and multiple calls
    for (const auto *param : *m->getParameters()) {
        cstring param_name = param->name.name;
        cstring merged_param_name = method_name + "_" + param_name;
        if (param->direction == IR::Direction::Out ||
            param->direction == IR::Direction::InOut) {
            auto *instance =
                state->gen_instance(merged_param_name, param->type, 0);
            // TODO: Clean up, this should not be necessary
            if (auto *si = instance->to_mut<StructBase>()) {
                si->bind();
                si->propagate_validity();
            }
            // Sometimes the parameter does not exist because of optional
            if (state->find_var(param_name) != nullptr) {
                state->update_var(param_name, instance);
            }
        }
    }
    auto *return_instance = state->gen_instance(method_name, method_type, 0);
    // TODO: Clean up, this should not be necessary
    if (auto *si = return_instance->to_mut<StructBase>()) {
        si->bind();
        si->propagate_validity();
    }
    state->set_expr_result(return_instance);
    return false;
}

bool Z3Visitor::preorder(const IR::EmptyStatement *) { return false; }

bool Z3Visitor::preorder(const IR::ReturnStatement *r) {
    auto forward_conds = state->get_forward_conds();
    auto return_conds = state->get_return_conds();
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (const auto &sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    for (const auto &sub_cond : return_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();
    // if we do not even return do not bother with collecting results
    if (r->expression != nullptr) {
        visit(r->expression);
        state->push_return_expr(cond && exit_cond, state->copy_expr_result());
    }
    state->push_return_state(cond && exit_cond, state->clone_vars());
    state->push_return_cond(!cond);
    state->set_returned(true);

    return false;
}

bool Z3Visitor::preorder(const IR::ExitStatement *) {
    auto forward_conds = state->get_forward_conds();
    auto return_conds = state->get_return_conds();
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (z3::expr &sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    for (z3::expr &sub_cond : return_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();

    auto scopes = state->get_state();
    auto old_state = state->clone_state();
    // Note the lack of leq in the i > 0 comparison.
    // We do not want to pop the last scope
    // FIXME: There has to be a cleaner way here...
    // Ideally we should track the input/output variables
    for (int64_t i = scopes.size() - 1; i > 0; --i) {
        const auto *scope = &scopes.at(i);
        auto copy_out_args = scope->get_copy_out_args();
        std::vector<P4Z3Instance *> copy_out_vals;
        for (const auto &arg_tuple : copy_out_args) {
            auto source = arg_tuple.second;
            auto *val = state->get_var(source);
            // Exit in parsers means that everything is invalid
            if (in_parser) {
                if (auto *si = val->to_mut<StructBase>()) {
                    auto invalid_bool = state->get_z3_ctx()->bool_val(false);
                    si->propagate_validity(&invalid_bool);
                }
            }
            copy_out_vals.push_back(val);
        }

        state->pop_scope();
        size_t idx = 0;
        for (auto &arg_tuple : copy_out_args) {
            auto target = arg_tuple.first;
            state->set_var(target, copy_out_vals[idx]);
            idx++;
        }
    }
    auto exit_vars = state->clone_vars();
    state->restore_state(old_state);
    state->add_exit_state(exit_cond && cond, exit_vars);
    state->set_exit_cond(exit_cond && !cond);
    state->set_exit(true);

    return false;
}

std::vector<std::pair<z3::expr, const IR::Statement *>>
collect_stmt_vec_table(Z3Visitor *visitor, const P4TableInstance *table,
                       const IR::Vector<IR::SwitchCase> &cases) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    std::vector<std::pair<z3::expr, const IR::Statement *>> stmt_vector;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    bool has_default = false;
    if (table->table_props.immutable) {
        std::vector<const P4Z3Instance *> evaluated_keys;
        for (const auto *key : table->table_props.keys) {
            // TODO: This should not be necessary
            // We have this information already
            visitor->visit(key->expression);
            const auto *key_eval = state->copy_expr_result();
            evaluated_keys.push_back(key_eval);
        }
        auto new_entries = table->table_props.entries;
        for (const auto *switch_case : cases) {
            if (const auto *label =
                    switch_case->label->to<IR::PathExpression>()) {
                z3::expr cond = ctx->bool_val(false);
                for (auto it = new_entries.begin(); it != new_entries.end();) {
                    auto entry = *it;
                    const auto *keys = entry.first;
                    const auto *action = entry.second;
                    if (label->toString() != action->method->toString()) {
                        ++it;
                        continue;
                    }
                    cond = cond || table->produce_const_match(
                                       visitor, &evaluated_keys, keys);
                    it = new_entries.erase(it);
                }
                // There is no block for the switch.
                // This expressions falls through to the next switch case.
                fall_through = fall_through || cond;
                if (switch_case->statement == nullptr) {
                    continue;
                }
                auto case_match = fall_through;
                // If the entries are empty we exhausted all possible matches
                // TODO: Not sure if this is a good idea?
                if (new_entries.empty()) {
                    case_match = ctx->bool_val(true);
                }
                // Matches the condition OR all the other fall-through switches
                fall_through = ctx->bool_val(false);
                matches = matches || case_match;
                stmt_vector.emplace_back(case_match, switch_case->statement);
            } else if (switch_case->label->is<IR::DefaultExpression>()) {
                has_default = true;
                stmt_vector.emplace_back(!matches, switch_case->statement);
            } else {
                P4C_UNIMPLEMENTED(
                    "Case expression %s of type %s not supported.",
                    switch_case->label, switch_case->label->node_type_name());
            }
        }
    } else {
        auto table_action_name = table->table_props.table_name + "action_idx";
        auto action_taken = ctx->int_const(table_action_name.c_str());
        std::map<cstring, int> action_mapping;
        size_t idx = 0;
        for (const auto *action : table->table_props.actions) {
            const auto *method_expr = action->method;
            const auto *path = method_expr->checkedTo<IR::PathExpression>();
            action_mapping[path->path->name.name] = idx;
            idx++;
        }
        // now actually map all the statements together
        z3::expr fall_through = ctx->bool_val(false);
        for (const auto *switch_case : cases) {
            if (const auto *label =
                    switch_case->label->to<IR::PathExpression>()) {
                auto mapped_idx = action_mapping[label->path->name.name];
                auto cond = action_taken == mapped_idx;
                // There is no block for the switch.
                // This expressions falls through to the next switch case.
                fall_through = fall_through || cond;
                if (switch_case->statement == nullptr) {
                    continue;
                }
                auto case_match = fall_through;
                // Matches the condition OR all the other fall-through switches
                fall_through = ctx->bool_val(false);
                matches = matches || case_match;
                stmt_vector.emplace_back(case_match, switch_case->statement);
            } else if (switch_case->label->is<IR::DefaultExpression>()) {
                has_default = true;
                stmt_vector.emplace_back(!matches, switch_case->statement);
                break;
            } else {
                P4C_UNIMPLEMENTED(
                    "Case expression %s of type %s not supported.",
                    switch_case->label, switch_case->label->node_type_name());
            }
        }
    }
    // If we did not encounter a default statement, implicitly add it
    if (!has_default) {
        stmt_vector.emplace_back(!matches, nullptr);
    }
    return stmt_vector;
}

std::vector<std::pair<z3::expr, const IR::Statement *>>
collect_stmt_vec_expr(Z3Visitor *visitor, const P4Z3Instance *switch_expr,
                      const IR::Vector<IR::SwitchCase> &cases) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    std::vector<std::pair<z3::expr, const IR::Statement *>> stmt_vector;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    bool has_default = false;
    for (const auto *switch_case : cases) {
        z3::expr cond = ctx->bool_val(true);
        if (switch_case->label->is<IR::DefaultExpression>()) {
            has_default = true;
            stmt_vector.emplace_back(!matches, switch_case->statement);
            break;
        }
        visitor->visit(switch_case->label);
        const auto *matched_expr = state->get_expr_result();
        cond = *switch_expr == *matched_expr;
        // There is no block for the switch.
        // This expressions falls through to the next switch case.
        fall_through = fall_through || cond;
        if (switch_case->statement == nullptr) {
            continue;
        }
        auto case_match = fall_through;
        // Matches the condition OR all the other fall-through switches
        fall_through = ctx->bool_val(false);
        matches = matches || case_match;
        stmt_vector.emplace_back(case_match, switch_case->statement);
    }
    // If we did not encounter a default statement, implicitly add it
    if (!has_default) {
        stmt_vector.emplace_back(!matches, nullptr);
    }
    return stmt_vector;
}

bool Z3Visitor::preorder(const IR::SwitchStatement *ss) {
    visit(ss->expression);
    const auto *switch_expr = state->get_expr_result();
    std::vector<std::pair<z3::expr, const IR::Statement *>> stmt_vector;

    // First map the individual statement blocks to their respective matches
    // Tables are a little complicated so we have to take special care
    if (const auto *table = switch_expr->to<P4TableInstance>()) {
        stmt_vector = collect_stmt_vec_table(this, table, ss->cases);
    } else {
        stmt_vector =
            collect_stmt_vec_expr(this, switch_expr->copy(), ss->cases);
    }

    // Once this is done we can use our usual ite-execution technique
    // We always add a default statement, so we can set exit/return to true
    bool has_exited = true;
    bool has_returned = true;
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    BUG_CHECK(!stmt_vector.empty(), "Statement vector can not be empty.");
    for (auto &stmt : stmt_vector) {
        auto case_match = stmt.first;
        const auto *case_stmt = stmt.second;
        auto old_vars = state->clone_vars();
        state->push_forward_cond(case_match);
        visit(case_stmt);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (!(call_has_exited || stmt_has_returned)) {
            case_states.emplace_back(case_match, state->get_vars());
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

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto z3_cond = state->get_expr_result<Z3Bitvector>()->get_val()->simplify();
    if (z3_cond.is_true()) {
        visit(ifs->ifTrue);
        return false;
    }
    if (z3_cond.is_false()) {
        visit(ifs->ifFalse);
        return false;
    }
    auto old_vars = state->clone_vars();
    state->push_forward_cond(z3_cond);
    visit(ifs->ifTrue);
    state->pop_forward_cond();
    auto then_has_exited = state->has_exited();
    auto then_has_returned = state->has_returned();
    VarMap then_vars;
    if (then_has_exited || then_has_returned) {
        then_vars = old_vars;
    } else {
        then_vars = state->clone_vars();
    }
    state->set_exit(false);
    state->set_returned(false);

    state->restore_vars(old_vars);
    auto old_state = state->clone_vars();
    state->push_forward_cond(!z3_cond);
    visit(ifs->ifFalse);
    state->pop_forward_cond();
    auto else_has_exited = state->has_exited();
    auto else_has_returned = state->has_returned();
    if (else_has_exited || else_has_returned) {
        state->restore_vars(old_state);
    }

    // If both branches have returned we set the if statement to returned or
    // exited
    state->set_exit(then_has_exited && else_has_exited);
    state->set_returned(then_has_returned && else_has_returned);
    state->merge_vars(z3_cond, then_vars);
    return false;
}

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (const auto *c : b->components) {
        visit(c);
        if (state->has_returned() || state->has_exited()) {
            break;
        }
    }
    return false;
}

bool Z3Visitor::preorder(const IR::MethodCallStatement *mcs) {
    visit(mcs->methodCall);
    return false;
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    state->set_var(this, as->left, as->right);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left = nullptr;
    const auto *resolved_type = state->resolve_type(dc->type);
    if (dc->initializer != nullptr) {
        visit(dc->initializer);
        left = state->get_expr_result()->cast_allocate(resolved_type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, resolved_type);
    }
    state->declare_var(dc->name.name, left, resolved_type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left = nullptr;
    const auto *resolved_type = state->resolve_type(dv->type);
    if (dv->initializer != nullptr) {
        visit(dv->initializer);
        left = state->get_expr_result()->cast_allocate(resolved_type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, resolved_type);
    }
    state->declare_var(dv->name.name, left, resolved_type);

    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    auto instance_name = di->name.name;
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (const auto *instance_decl = resolved_type->to<IR::Type_Declaration>()) {
        const IR::ParameterList *params = nullptr;
        const IR::TypeParameters *type_params = nullptr;
        if (const auto *c = instance_decl->to<IR::P4Control>()) {
            params = c->getConstructorParameters();
            type_params = c->getTypeParameters();
        } else if (const auto *p = instance_decl->to<IR::P4Parser>()) {
            params = p->getConstructorParameters();
            type_params = p->getTypeParameters();
        } else {
            P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                              resolved_type, resolved_type->node_type_name());
        }
        auto var_map = state->merge_args_with_params(this, *di->arguments,
                                                     *params, *type_params);
        state->declare_var(
            instance_name,
            new ControlInstance(state, instance_decl, var_map.second),
            resolved_type);
    } else {
        P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ",
                          resolved_type, resolved_type->node_type_name());
    }
    return false;
}

}  // namespace TOZ3
