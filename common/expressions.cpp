#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (auto tb = c->type->to<IR::Type_Bits>()) {
        auto val_string = Util::toString(c->value, 0, false);
        Z3Bitvector wrapper = Z3Bitvector(
            state->get_z3_ctx()->bv_val(val_string, tb->size), tb->isSigned);
        state->set_expr_result(wrapper);
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto var = Z3Int(state->get_z3_ctx()->int_val(val_string));
        state->set_expr_result(var);
        return false;
    }
    BUG("Constant Node %s not implemented!", c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::BoolLiteral *bl) {
    Z3Bitvector wrapper = Z3Bitvector(state->get_z3_ctx()->bool_val(bl->value));
    state->set_expr_result(wrapper);
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

std::function<void(void)> get_method_member(Z3Visitor *visitor,
                                            const IR::Member *member) {
    visitor->visit(member->expr);
    P4Z3Instance *complex_class = visitor->state->get_expr_result();
    if (auto si = complex_class->to_mut<StructBase>()) {
        return si->get_function(member->member.name);
    }
    P4C_UNIMPLEMENTED("Method member not supported for %s.", member);
}

const IR::ParameterList *get_params(const IR::Declaration *callable) {
    if (auto p4action = callable->to<IR::P4Action>()) {
        return p4action->getParameters();
    } else if (auto fun = callable->to<IR::Function>()) {
        return fun->getParameters();
    } else if (auto fun = callable->to<IR::Method>()) {
        return fun->getParameters();
    } else {
        P4C_UNIMPLEMENTED(
            "Callable declaration type %s of type %s not supported.", callable,
            callable->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Declaration *callable;
    const IR::ParameterList *params;

    const IR::Expression *method_type = mce->method;

    if (auto path_expr = method_type->to<IR::PathExpression>()) {
        cstring name = path_expr->path->name.name;
        callable = state->get_static_decl(name)->decl;
        params = get_params(callable);
    } else if (auto member = method_type->to<IR::Member>()) {
        visit(member->expr);
        // try to resolve the normal way and find a function pointer
        P4Z3Instance *result = state->get_expr_result();
        if (auto si = result->to_mut<HeaderInstance>()) {
            // call the function directly for now
            si->get_function(member->member.name)();
            return false;
        } else if (auto p4extern = result->to_mut<ExternInstance>()) {
            callable = p4extern->get_method(member->member.name);
            params = get_params(callable);
        } else if (auto decl = result->to_mut<P4Declaration>()) {
            callable = decl->decl;
            params = get_params(callable);
        } else {
            P4C_UNIMPLEMENTED("Method member call %s not supported.", mce);
        }
    } else {
        P4C_UNIMPLEMENTED("Method call %s not supported.", mce);
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
            if (auto z3_var = var->to<Z3Bitvector>()) {
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
