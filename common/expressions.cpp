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
        auto expr = state->get_z3_ctx()->bv_val(val_string, tb->size);
        auto wrapper = Z3Bitvector(state, expr, tb->isSigned);
        state->set_expr_result(wrapper);
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = state->get_z3_ctx()->int_val(val_string);
        auto var = Z3Int(state, expr);
        state->set_expr_result(var);
        return false;
    }
    P4C_UNIMPLEMENTED("Constant Node %s not implemented!",
                      c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::BoolLiteral *bl) {
    auto expr = state->get_z3_ctx()->bool_val(bl->value);
    Z3Bitvector wrapper = Z3Bitvector(state, expr);
    state->set_expr_result(wrapper);
    return false;
}

bool Z3Visitor::preorder(const IR::NamedExpression *ne) {
    // TODO: Figure out what the implications of a name are here...
    visit(ne->expression);
    return false;
}
bool Z3Visitor::preorder(const IR::ListExpression *le) {
    std::vector<P4Z3Instance *> members;
    for (auto component : le->components) {
        visit(component);
        members.push_back(state->copy_expr_result());
    }
    state->set_expr_result(new ListInstance(state, members, le->type));
    return false;
}

bool Z3Visitor::preorder(const IR::StructExpression *se) {
    std::vector<P4Z3Instance *> members;
    for (auto component : se->components) {
        visit(component);
        members.push_back(state->copy_expr_result());
    }
    // TODO: Not sure what the deal with Type_Unknown is here
    if (se->type && !se->type->is<IR::Type_Unknown>()) {
        auto instance = state->gen_instance("undefined", se->type);
        if (auto struct_instance = instance->to_mut<StructBase>()) {
            struct_instance->set_list(members);
        } else {
            P4C_UNIMPLEMENTED("Unsupported StructExpression class %s", se);
        }
        state->set_expr_result(instance);
    } else {
        state->set_expr_result(new ListInstance(state, members, se->type));
    }
    return false;
}

bool Z3Visitor::preorder(const IR::PathExpression *p) {
    state->set_expr_result(state->get_var(p->path->name));
    return false;
}

std::vector<std::pair<const IR::Expression *, cstring>>
resolve_args(const IR::Vector<IR::Argument> *args,
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

P4Z3Instance *resolve_var_or_decl_parent(Z3Visitor *visitor,
                                         const IR::Member *m) {
    const IR::Expression *parent = m->expr;
    if (auto member = parent->to<IR::Member>()) {
        visitor->visit(member);
        return visitor->state->get_expr_result();
    } else if (auto path_expr = parent->to<IR::PathExpression>()) {
        P4Scope *scope;
        cstring name = path_expr->path->name.name;
        if (auto decl = visitor->state->find_static_decl(name, &scope)) {
            return decl;
        } else {
            // try to find the result in vars and fail otherwise
            return visitor->state->get_var(name);
        }
    }
    P4C_UNIMPLEMENTED("Parent %s of type %s not implemented!", parent,
                      parent->node_type_name());
}

const IR::ParameterList *get_params(const IR::Declaration *callable) {
    if (auto p4action = callable->to<IR::P4Action>()) {
        return p4action->getParameters();
    } else if (auto fun = callable->to<IR::Function>()) {
        return fun->getParameters();
    } else if (auto method = callable->to<IR::Method>()) {
        return method->getParameters();
    } else if (auto table = callable->to<IR::P4Table>()) {
        return table->getApplyParameters();
    } else {
        P4C_UNIMPLEMENTED(
            "Callable declaration type %s of type %s not supported.", callable,
            callable->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Declaration *callable;
    const IR::ParameterList *params;

    auto method_type = mce->method;

    if (auto path_expr = method_type->to<IR::PathExpression>()) {
        cstring name = path_expr->path->name.name;
        callable = state->get_static_decl(name)->decl;
        params = get_params(callable);
    } else if (auto member = method_type->to<IR::Member>()) {
        // try to resolve and find a function pointer
        auto result = resolve_var_or_decl_parent(this, member);
        if (auto si = result->to_mut<HeaderInstance>()) {
            // call the function directly for now
            si->get_function(member->member.name)();
            return false;
        } else if (auto p4t = result->to_mut<P4TableInstance>()) {
            // call the function directly for now
            // FIXME: This ignores arguments...
            p4t->get_function(member->member.name)();
            auto thing = state->copy_expr_result<P4TableInstance>();
            visit(thing->decl);
            state->set_expr_result(thing);
            return false;
        } else if (auto p4extern = result->to_mut<ExternInstance>()) {
            callable = p4extern->get_method(member->member.name);
            params = get_params(callable);
        } else if (auto decl = result->to_mut<P4Declaration>()) {
            callable = decl->decl;
            params = get_params(callable);
        } else {
            P4C_UNIMPLEMENTED("Method member call %s of type %s not supported.",
                              result->to_string(), result->get_static_type());
        }
    } else {
        P4C_UNIMPLEMENTED("Method call %s not supported.", mce);
    }
    std::vector<std::pair<const IR::Expression *, cstring>> copy_out_args =
        resolve_args(mce->arguments, params);
    auto merged_args = merge_args_with_params(mce->arguments, params);

    state->push_scope();
    for (auto arg_tuple : merged_args) {
        cstring param_name = arg_tuple.first;
        auto arg_val = arg_tuple.second;
        state->declare_var(param_name, arg_val.first, arg_val.second);
    }
    visit(callable);
    auto expr_result = state->copy_expr_result();
    std::vector<P4Z3Instance *> copy_out_vals;
    for (auto arg_tuple : copy_out_args) {
        auto source = arg_tuple.second;
        auto val = state->get_var(source);
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
            auto var = state->gen_instance(param->name.name, par_type);
            if (auto z3_var = var->to_mut<StructBase>()) {
                z3_var->propagate_validity();
            }
            state->declare_var(param->name.name, var, par_type);
            state_names.push_back(param->name.name);
        }

        // VISIT THE CONTROL
        visit(resolved_type);

        // Merge the exit states
        for (auto exit_tuple : state->exit_states) {
            state->merge_state(exit_tuple.first, exit_tuple.second);
        }
        // Clear the exit states
        state->exit_states.clear();

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

    auto ctrl_state = new ControlState(state_vars);
    state->set_expr_result(ctrl_state);

    return false;
}

} // namespace TOZ3_V2
