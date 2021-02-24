#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "complex_type.h"
#include "lib/exceptions.h"
#include "scope.h"
#include "state.h"
#include "z3_int.h"
#include "z3_interpreter.h"

namespace TOZ3_V2 {

P4Z3Instance Z3Visitor::cast(P4Z3Instance expr, const IR::Type *dest_type) {
    if (auto tb = dest_type->to<IR::Type_Bits>()) {
        if (z3::expr *z3_var = boost::get<z3::expr>(&expr)) {
            if (z3_var->get_sort().is_bv()) {
                return state->ctx->bv_val(z3_var->get_decimal_string(0).c_str(),
                                          dest_type->width_bits());
            } else {
                BUG("Cast type not supported ");
            }
        } else if (auto z3_var = check_complex<Z3Int>(expr)) {
            auto val_string = z3_var->val.get_decimal_string(0);
            return state->ctx->bv_val(val_string.c_str(),
                                      dest_type->width_bits());
        } else {
            BUG("Cast from expr xpr to node %s supported ",
                dest_type->node_type_name());
        }
    } else {
        BUG("Cast to type %s not supported", dest_type->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    auto left = state->return_expr;
    visit(expr->right);
    auto right = state->return_expr;

    if (z3::expr *z3_left_var = boost::get<z3::expr>(&left)) {
        if (z3::expr *z3_right_var = boost::get<z3::expr>(&right)) {
            state->return_expr = *z3_left_var == *z3_right_var;
        } else {
            BUG("Z3 eq with int not yet supported. ");
        }
    } else if (auto z3_var = check_complex<Z3Int>(left)) {
        BUG("Int eq not supported. ");
    } else if (auto z3_left = check_complex<StructInstance>(left)) {
        BUG("Z3 Struct eq not yet supported. ");
    } else {
        BUG("Eq not supported. ");
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (auto tb = c->type->to<IR::Type_Bits>()) {
        if (tb->isSigned) {
            state->return_expr = state->create_int(c->value, tb->size);
        } else {
            auto val_string = Util::toString(c->value, 0, false);
            state->return_expr = state->ctx->bv_val(val_string, tb->size);
        }
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        state->return_expr = state->create_int(c->value, -1);
        return false;
    }
    BUG("Constant Node %s not implemented!", c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::PathExpression *p) {
    P4Scope *scope;
    state->return_expr = state->find_var(p->path->name, &scope);
    return false;
}

bool Z3Visitor::preorder(const IR::Cast *c) {
    // resolve expression
    visit(c->expr);
    auto resolved_expr = state->return_expr;
    state->return_expr = cast(resolved_expr, c->destType);
    return false;
}

bool Z3Visitor::preorder(const IR::Member *m) {
    P4Z3Instance complex_class = nullptr;
    const IR::Expression *parent = m->expr;
    if (auto member = parent->to<IR::Member>()) {
        visit(member);
        complex_class = state->return_expr;
    } else if (auto name = parent->to<IR::PathExpression>()) {
        complex_class = state->get_var(name->path->name);
    } else {
        BUG("Parent Type  %s not implemented!", parent->node_type_name());
    }
    StructInstance *si = check_complex<StructInstance>(complex_class);
    if (not si) {
        BUG("Can not cast to StructInstance.");
    }

    state->return_expr = si->get_var(m->member.name);
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
            if (auto path_expr = arg->to<IR::Member>()) {
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
    P4Z3Instance return_expr = nullptr;

    if (auto path_expr = mce->method->to<IR::PathExpression>()) {
        P4Declaration *method_decl =
            state->get_var<P4Declaration>(path_expr->path->name.name);
        if (auto p4action = method_decl->decl->to<IR::P4Action>()) {
            callable = p4action;
            params = p4action->getParameters();
        } else {
            BUG("Method type %s not supported.",
                method_decl->decl->node_type_name());
        }
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
        P4Z3Instance arg_val = arg_tuple.second;
        state->declare_local_var(param_name, arg_val);
    }
    visit(callable);
    return_expr = state->return_expr;

    std::vector<P4Z3Instance> copy_out_vals;
    for (auto arg_tuple :copy_out_args) {
        auto source = arg_tuple.second;
        P4Z3Instance val = state->get_var(source);
        copy_out_vals.push_back(val);
    }
    state->pop_scope();
    size_t idx = 0;
    for (auto arg_tuple : copy_out_args) {
        auto target = arg_tuple.first;
        set_var(target, copy_out_vals[idx]);
        idx++;
    }
    state->return_expr = return_expr;
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
            P4Z3Instance var = state->gen_instance(param->name.name, par_type);
            state->declare_local_var(param->name.name, var);
            state_names.push_back(param->name.name);
        }

        // VISIT THE CONTROL
        visit(resolved_type);

        // COLLECT
        for (auto state_name : state_names) {
            P4Scope *scope;
            auto member = state->find_var(state_name, &scope);
            if (z3::expr *z3_var = boost::get<z3::expr>(&member)) {
                state_vars.push_back({state_name, *z3_var});
            } else if (auto z3_var = check_complex<StructInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (auto z3_var = check_complex<ErrorInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (auto z3_var = check_complex<EnumInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (check_complex<ExternInstance>(member)) {
                printf("Skipping extern...\n");
            } else {
                BUG("Var is neither type z3::expr nor P4ComplexInstance!");
            }
        }
    }
    state->pop_scope();

    // FIXME: Figure out when and how to free this
    auto ctrl_state = new ControlState(state_vars);
    // state->add_to_allocated(ctrl_state);
    state->return_expr = ctrl_state;

    return false;
}

} // namespace TOZ3_V2
