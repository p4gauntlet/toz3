#include <z3++.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "ir/ir-generated.h"
#include "ir/ir.h"
#include "ir/vector.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/exceptions.h"
#include "lib/ordered_map.h"
#include "state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/type_simple.h"
#include "toz3/common/util.h"
#include "type_complex.h"

namespace P4::ToZ3 {
/***
===============================================================================
P4TableInstance
===============================================================================
***/

void process_table_properties(const IR::P4Table *p4t, TableProperties *table_props) {
    if (const auto *key_prop = p4t->getKey()) {
        for (const auto *ke : key_prop->keyElements) {
            table_props->keys.push_back(ke);
        }
    }
    if (const auto *action_list = p4t->getActionList()) {
        for (const auto *act : action_list->actionList) {
            bool isDefault = false;
            for (const auto *anno : act->getAnnotations()) {
                if (anno->name.name == "defaultonly") {
                    isDefault = true;
                    break;
                }
            }
            if (isDefault) {
                continue;
            }
            if (const auto *method_call = act->expression->to<IR::MethodCallExpression>()) {
                table_props->actions.push_back(method_call);
            } else if (act->expression->is<IR::PathExpression>()) {
                table_props->actions.push_back(new IR::MethodCallExpression(act->expression));
            } else {
                P4C_UNIMPLEMENTED("Unsupported action entry %s of type %s", act,
                                  act->expression->node_type_name());
            }
        }
    }
    if (const auto *default_expr = p4t->getDefaultAction()) {
        // resolve a default action
        if (const auto *method_call = default_expr->to<IR::MethodCallExpression>()) {
            table_props->default_action = method_call;
        } else if (default_expr->is<IR::PathExpression>()) {
            table_props->default_action = new IR::MethodCallExpression(default_expr);
        } else {
            P4C_UNIMPLEMENTED("Unsupported expression value %s of type %s", default_expr,
                              default_expr->node_type_name());
        }
    }
    if (const auto *entries = p4t->getEntries()) {
        // If the entries properties is constant it means the entries are fixed
        // We cannot add or remove table entries
        table_props->immutable = p4t->properties->getProperty("entries"_cs)->isConstant;
        for (const auto *entry : entries->entries) {
            const auto *action_expr = entry->getAction();
            const IR::MethodCallExpression *action = nullptr;
            if (const auto *method_call = action_expr->to<IR::MethodCallExpression>()) {
                action = method_call;
            } else if (action_expr->is<IR::PathExpression>()) {
                action = new IR::MethodCallExpression(action_expr);
            } else {
                P4C_UNIMPLEMENTED("Unsupported constant action entry %s of type %s", action_expr,
                                  action_expr->node_type_name());
            }
            table_props->entries.emplace_back(entry->getKeys(), action);
        }
    }
}

P4TableInstance::P4TableInstance(P4State *state, const IR::P4Table *p4t)
    : P4Declaration(p4t), state(state), hit(state->get_z3_ctx()->bool_val(false)) {
    members.insert({"action_run"_cs, this});
    members.insert({"hit"_cs, new Z3Bitvector(state, &BOOL_TYPE, hit)});
    members.insert({"miss"_cs, new Z3Bitvector(state, &BOOL_TYPE, !hit)});
    cstring apply_str = mangle_name(cstring("apply"), p4t->getApplyParameters()->size());
    add_function(apply_str, [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
        apply(visitor, args);
    });

    table_props.table_name = infer_name(p4t, p4t->name.name);
    // We first collect all the necessary properties
    process_table_properties(p4t, &table_props);
    // Also check if the table is invisible to the control plane.
    // This also implies that it cannot be modified.
    table_props.immutable = p4t->hasAnnotation(IR::Annotation::hiddenAnnotation);
}

P4TableInstance::P4TableInstance(P4State *state, const IR::StatOrDecl *decl, z3::expr hit,
                                 TableProperties table_props)
    : P4Declaration(decl), state(state), hit(hit), table_props(std::move(table_props)) {
    members.insert({"action_run"_cs, this});
    members.insert({"hit"_cs, new Z3Bitvector(state, &BOOL_TYPE, hit)});
    members.insert({"miss"_cs, new Z3Bitvector(state, &BOOL_TYPE, !hit)});
    cstring apply_str = "apply"_cs;
    if (const auto *table = decl->to<IR::P4Table>()) {
        apply_str = mangle_name(apply_str, table->getApplyParameters()->size());
    }
    add_function(apply_str, [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
        apply(visitor, args);
    });
}

z3::expr compute_table_hit(Visitor *visitor, P4State *state, cstring table_name,
                           const std::vector<const IR::KeyElement *> &keys,
                           std::vector<const P4Z3Instance *> *evaluated_keys) {
    auto *ctx = state->get_z3_ctx();
    z3::expr hit = ctx->bool_val(false);
    for (std::size_t idx = 0; idx < keys.size(); ++idx) {
        const auto *key = keys.at(idx);
        // TODO: Actually look up the match type here. Not sure why needed...
        visitor->visit(key->expression);
        const auto *key_eval = state->copy_expr_result();
        evaluated_keys->push_back(key_eval);
        const auto *val_container = key_eval->to<ValContainer>();
        BUG_CHECK(val_container,
                  "Key type %s not "
                  "supported for tables.",
                  key_eval->get_static_type());
        cstring key_name = table_name + "_table_key_" + std::to_string(idx);
        const auto key_eval_z3 = val_container->get_val()->simplify();
        const auto key_z3_sort = key_eval_z3.get_sort();
        const auto key_match = ctx->constant(key_name.c_str(), key_z3_sort);
        // It is actually possible to use a variety of types as key.
        // So we have to stay generic and produce a corresponding variable.
        cstring key_string = key->matchType->toString();
        if (key_string == "exact") {
            hit = hit || (key_eval_z3 == key_match);
        } else if (key_string == "lpm") {
            // FIXME: switch to abseil routines for string manipulations
            cstring mask_name = table_name + "_table_lpm_key_" + std::to_string(idx);
            const auto mask_var = ctx->constant(mask_name.c_str(), key_z3_sort);
            auto max_return =
                ctx->bv_val(get_max_bv_val(key_z3_sort.bv_size()).c_str(), key_z3_sort.bv_size());
            auto lpm_mask = z3::shl(max_return, mask_var).simplify();
            hit = hit || (key_eval_z3 & lpm_mask) == (key_match & lpm_mask);
        } else if (key_string == "ternary") {
            cstring mask_name = table_name + "_table_ternary_key_" + std::to_string(idx);
            const auto mask_var = ctx->constant(mask_name.c_str(), key_z3_sort);
            hit = hit || (key_eval_z3 & mask_var) == (key_match & mask_var);
        } else if (key_string == "range") {
            cstring min_name = table_name + "_table_min_" + std::to_string(idx);
            cstring max_name = table_name + "_table_max_" + std::to_string(idx);
            auto *min_key = state->gen_instance(min_name, key_eval->get_p4_type());
            auto *max_key = state->gen_instance(max_name, key_eval->get_p4_type());
            hit = hit ||
                  ((*min_key < *max_key) && (*min_key <= *key_eval) && (*key_eval <= *max_key));
        } else if (key_string == "optional" || key_string == "selector") {
            // TODO: Not sure what to do with these?
        } else {
            P4C_UNIMPLEMENTED("Match type %s not implemented for table keys.", key_string);
        }
    }
    return hit;
}

void handle_table_action(Visitor *visitor, P4State *state, const IR::MethodCallExpression *act,
                         cstring action_label) {
    const IR::Expression *call_name = nullptr;
    IR::Vector<IR::Argument> ctrl_args;
    const IR::ParameterList *method_params = nullptr;

    if (const auto *path = act->method->to<IR::PathExpression>()) {
        call_name = path;
        cstring identifier_path = path->path->name + std::to_string(act->arguments->size());
        const auto *action_decl = state->get_static_decl(identifier_path);
        if (const auto *action = action_decl->get_decl()->to<IR::P4Action>()) {
            method_params = action->getParameters();
        } else {
            BUG("Unexpected action call %s of type %s in table.", action_decl->get_decl(),
                action_decl->get_decl()->node_type_name());
        }
    } else {
        P4C_UNIMPLEMENTED("Unsupported action %s of type %s", act, act->method->node_type_name());
    }
    for (const auto &arg : *act->arguments) {
        ctrl_args.push_back(arg);
    }
    // At this stage, we synthesize control plane arguments
    // TODO: Simplify this.
    auto args_len = act->arguments->size();
    auto ctrl_idx = 0;
    for (size_t idx = 0; idx < method_params->size(); ++idx) {
        const auto *param = method_params->getParameter(idx);
        if (args_len <= idx && param->direction == IR::Direction::None) {
            cstring arg_name = action_label + std::to_string(ctrl_idx);
            auto *ctrl_arg = state->gen_instance(arg_name, param->type);
            // TODO: This is a bug waiting to happen. How to handle fresh
            // arguments and their source?
            state->declare_var(arg_name, ctrl_arg, param->type);
            ctrl_args.push_back(new IR::Argument(new IR::PathExpression(arg_name)));
            ctrl_idx++;
        }
    }

    const auto *action_with_ctrl_args = new IR::MethodCallExpression(call_name, &ctrl_args);
    visitor->visit(action_with_ctrl_args);
}

z3::expr P4TableInstance::produce_const_match(Visitor *visitor,
                                              std::vector<const P4Z3Instance *> *evaluated_keys,
                                              const IR::ListExpression *entry_keys) const {
    z3::expr match = state->get_z3_ctx()->bool_val(true);
    for (size_t idx = 0; idx < evaluated_keys->size(); ++idx) {
        const auto *key_eval = evaluated_keys->at(idx);
        const auto *c_key = entry_keys->components.at(idx);
        if (c_key->is<IR::DefaultExpression>()) {
            continue;
        }
        if (const auto *range = c_key->to<IR::Range>()) {
            visitor->visit(range->left);
            const auto *min = state->copy_expr_result();
            visitor->visit(range->right);
            const auto *max = state->get_expr_result();
            match = match && (*min <= *key_eval && *key_eval <= *max);
        } else if (const auto *mask_expr = c_key->to<IR::Mask>()) {
            visitor->visit(mask_expr->left);
            const auto *val = state->copy_expr_result();
            visitor->visit(mask_expr->right);
            const auto *mask = state->get_expr_result();
            match = match && (*(*key_eval & *mask) == *(*val & *mask));
        } else {
            visitor->visit(c_key);
            match = match && (*key_eval == *state->get_expr_result());
        }
    }
    return match;
}
void P4TableInstance::apply(Visitor *visitor, const IR::Vector<IR::Argument> *args) {
    auto *ctx = state->get_z3_ctx();
    const auto *table_decl = get_decl()->checkedTo<IR::P4Table>();
    const auto *params = table_decl->getApplyParameters();
    const auto *type_params = table_decl->getApplyMethodType()->getTypeParameters();
    const ParamInfo param_info = {*params, *args, *type_params, {}};
    state->copy_in(visitor, param_info);

    std::vector<const P4Z3Instance *> evaluated_keys;
    z3::expr new_hit =
        compute_table_hit(visitor, state, table_props.table_name, table_props.keys, &evaluated_keys)
            .simplify();

    std::vector<std::pair<z3::expr, VarMap>> action_vars;
    bool has_exited = true;

    z3::expr matches = state->get_z3_ctx()->bool_val(false);
    // Skip all of this if we do not even match
    if (!new_hit.is_false()) {
        uint64_t idx = 0;
        // First the constant entries
        for (const auto &entry : table_props.entries) {
            const auto *keys = entry.first;
            const auto *action = entry.second;
            auto key_match = produce_const_match(visitor, &evaluated_keys, keys);
            auto cond = new_hit && (key_match);
            auto old_vars = state->clone_vars();
            state->push_forward_cond(cond);
            auto action_label = table_props.table_name + std::to_string(idx);
            handle_table_action(visitor, state, action, action_label);
            state->pop_forward_cond();
            auto call_has_exited = state->has_exited();
            if (!call_has_exited) {
                action_vars.emplace_back(cond, state->get_vars());
            }
            has_exited = has_exited && call_has_exited;
            state->set_exit(false);
            state->restore_vars(old_vars);
            matches = matches || cond;
            idx++;
        }
        // Then the actions
        if (!table_props.immutable) {
            auto table_action_name = table_props.table_name + "action_idx";
            auto table_action = ctx->int_const(table_action_name.c_str());
            for (const auto *action : table_props.actions) {
                auto cond = new_hit && (table_action == state->get_z3_ctx()->int_val(idx));
                auto old_vars = state->clone_vars();
                state->push_forward_cond(cond);
                auto action_label = table_props.table_name + std::to_string(idx);
                handle_table_action(visitor, state, action, action_label);
                state->pop_forward_cond();
                auto call_has_exited = state->has_exited();
                if (!call_has_exited) {
                    action_vars.emplace_back(cond, state->get_vars());
                }
                has_exited = has_exited && call_has_exited;
                state->set_exit(false);
                state->restore_vars(old_vars);
                matches = matches || cond;
                idx++;
            }
        }
    }

    if (table_props.default_action != nullptr) {
        auto old_vars = state->clone_vars();
        state->push_forward_cond(!hit || !matches);
        auto action_label = table_props.table_name + "default";
        handle_table_action(visitor, state, table_props.default_action, action_label);
        state->pop_forward_cond();
        if (state->has_exited()) {
            state->restore_vars(old_vars);
        }
    }
    state->set_exit(has_exited && state->has_exited());

    for (auto it = action_vars.rbegin(); it != action_vars.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    state->set_expr_result(new P4TableInstance(state, get_decl(), new_hit, table_props));

    state->copy_out();
}
}  // namespace P4::ToZ3
