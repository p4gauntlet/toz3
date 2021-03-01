#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (auto tb = c->type->to<IR::Type_Bits>()) {
        if (tb->isSigned) {
            state->set_expr_result(state->create_int(c->value, tb->size));
        } else {
            auto val_string = Util::toString(c->value, 0, false);
            state->set_expr_result(state->get_z3_ctx()->bv_val(val_string, tb->size));
        }
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        state->set_expr_result(state->create_int(c->value, 0));
        return false;
    }
    BUG("Constant Node %s not implemented!", c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::BoolLiteral *bl) {
    state->set_expr_result(state->get_z3_ctx()->bool_val(bl->value));
    return false;
}

bool Z3Visitor::preorder(const IR::PathExpression *p) {
    state->set_expr_result(state->get_var(p->path->name));
    return false;
}

std::vector<std::pair<const IR::Expression *, cstring>>
Z3Visitor::resolve_args(const IR::Vector<IR::Argument> *args,
                        const IR::ParameterList *params) {
    std::vector<std::pair<const IR::Expression *, cstring>> resolved_args;

    size_t arg_len = args->size();
    size_t idx = 0;
    for (auto param : params->parameters) {
        auto direction = param->direction;
        if (direction == IR::Direction::In ||
            direction == IR::Direction::None) {
            continue;
        }
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            if (arg->to<IR::Member>()) {
                // TODO: Index
                resolved_args.push_back({arg->expression, param->name.name});
            } else {
                resolved_args.push_back({arg->expression, param->name.name});
            }
        }
        idx++;
    }
    return resolved_args;
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Declaration *callable;
    const IR::ParameterList *params;

    if (auto path_expr = mce->method->to<IR::PathExpression>()) {
        P4Declaration *method_decl =
            state->get_var<P4Declaration>(path_expr->path->name.name);
        if (auto p4action = method_decl->decl->to<IR::P4Action>()) {
            callable = p4action;
            params = p4action->getParameters();
        } else if (auto fun = method_decl->decl->to<IR::Function>()) {
            callable = fun;
            params = fun->getParameters();
        } else {
            BUG("Method type %s not supported.",
                method_decl->decl->node_type_name());
        }
    } else if (auto member = mce->method->to<IR::Member>()) {
        auto method = get_method_member(member);
        method();
        return false;
    } else {
        BUG("Method reference %s not supported.",
            mce->method->node_type_name());
    }
    std::vector<std::pair<const IR::Expression *, cstring>> copy_out_args =
        resolve_args(mce->arguments, params);
    P4Z3Result merged_args = merge_args_with_params(mce->arguments, params);

    state->push_scope();
    for (auto arg_tuple : merged_args) {
        cstring param_name = arg_tuple.first;
        P4Z3Instance *arg_val = arg_tuple.second;
        state->declare_local_var(param_name, arg_val);
    }
    visit(callable);
    P4Z3Instance *expr_result = state->get_expr_result();
    std::vector<P4Z3Instance *> copy_out_vals;
    for (auto arg_tuple : copy_out_args) {
        auto source = arg_tuple.second;
        P4Z3Instance *val = state->get_var(source);
        copy_out_vals.push_back(val);
    }
    state->pop_scope();
    size_t idx = 0;
    for (auto arg_tuple : copy_out_args) {
        auto target = arg_tuple.first;
        set_var(target, copy_out_vals[idx]);
        idx++;
    }
    state->set_expr_result(expr_result);
    return false;
}

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    state->push_scope();
    if (auto c = resolved_type->to<IR::P4Control>()) {
        std::vector<cstring> state_names;
        // INITIALIZE
        for (auto param : *c->getApplyParameters()) {
            auto par_type = state->resolve_type(param->type);
            P4Z3Instance *var = state->gen_instance(param->name.name, par_type);
            if (auto z3_var = var->to_mut<StructBase>()) {
                z3_var->propagate_validity();
            }
            state->declare_local_var(param->name.name, var);
            state_names.push_back(param->name.name);
        }

        // VISIT THE CONTROL
        visit(resolved_type);

        // COLLECT
        for (auto state_name : state_names) {
            auto var = state->get_var(state_name);
            if (auto z3_var = var->to<Z3Wrapper>()) {
                state_vars.push_back({state_name, z3_var->val});
            } else if (auto z3_var = var->to<StructBase>()) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (var->to<ExternInstance>()) {
                printf("Skipping extern...\n");
            } else {
                BUG("Var is neither type z3::expr nor P4Z3Instance!");
            }
        }
    }
    state->pop_scope();

    // FIXME: Figure out when and how to free this
    P4Z3Instance *ctrl_state = new ControlState(state_vars);
    // state->add_to_allocated(ctrl_state);
    state->set_expr_result(ctrl_state);

    return false;
}

} // namespace TOZ3_V2
