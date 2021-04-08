
#include <cstdio>
#include <iostream>
#include <utility>

#include "z3++.h"
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

bool Z3Visitor::preorder(const IR::TypeNameExpression *t) {
    state->set_expr_result(state->get_var(t->typeName->path->name));
    return false;
}

void resolve_stack_call(Visitor *visitor, P4State *state,
                        MemberStruct &member_struct,
                        const IR::Vector<IR::Argument> *arguments) {
    auto arg_size = arguments->size();
    auto hdr_pairs = get_hdr_pairs(state, member_struct);

    // Execute and merge the functions
    if (auto name = boost::get<cstring>(&member_struct.target_member)) {
        std::vector<std::pair<z3::expr, VarMap>> call_vars;
        for (auto &parent_pair : hdr_pairs) {
            auto cond = parent_pair.first;
            auto parent_class = parent_pair.second;
            auto member_identifier = *name + std::to_string(arg_size);
            auto resolved_call = parent_class->get_function(member_identifier);
            if (auto function = resolved_call->to<FunctionWrapper>()) {
                // TODO: Support global side effects
                // For now, we only merge with the current class
                // There is some strange behavior here when using all state
                auto orig_class = parent_class->copy();
                function->function_call(visitor, arguments);
                parent_class->merge(!cond, *orig_class);
            } else {
                BUG("Unexpected stack call member %s ",
                    resolved_call->get_static_type());
            }
        }
    } else {
        P4C_UNIMPLEMENTED("Member type not implemented.");
    }
}

const P4Z3Instance *
resolve_var_or_decl_parent(P4State *state, const MemberStruct &member_struct,
                           int num_args) {
    const P4Z3Instance *parent_class;
    P4Scope *scope;
    if (auto decl =
            state->find_static_decl(member_struct.main_member, &scope)) {
        parent_class = decl;
    } else {
        // try to find the result in vars and fail otherwise
        parent_class = state->get_var(member_struct.main_member);
    }

    for (auto it = member_struct.mid_members.rbegin();
         it != member_struct.mid_members.rend(); ++it) {
        auto mid_member = *it;
        if (auto name = boost::get<cstring>(&mid_member)) {
            parent_class = parent_class->get_member(*name);
        } else {
            P4C_UNIMPLEMENTED("Member type not supported.");
        }
    }
    if (auto name = boost::get<cstring>(&member_struct.target_member)) {
        // FIXME: This is a very rough version of overloading...
        auto member_identifier = *name + std::to_string(num_args);
        return parent_class->get_function(member_identifier);
    }
    P4C_UNIMPLEMENTED("Member type not implemented.");
}

const IR::ParameterList *get_params(const IR::Node *callable) {
    if (auto p4action = callable->to<IR::P4Action>()) {
        return p4action->getParameters();
    } else if (auto fun = callable->to<IR::Function>()) {
        return fun->getParameters();
    } else if (auto method = callable->to<IR::Method>()) {
        return method->getParameters();
    } else {
        P4C_UNIMPLEMENTED("Callable declaration %s of type %s not supported.",
                          callable, callable->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Node *callable;
    const IR::ParameterList *params;
    auto arguments = mce->arguments;
    auto arg_size = arguments->size();

    auto method_type = mce->method;
    if (auto path_expr = method_type->to<IR::PathExpression>()) {
        // FIXME: This is a very rough version of overloading...
        auto path_identifier =
            path_expr->path->name.name + std::to_string(arg_size);
        callable = state->get_static_decl(path_identifier)->decl;
    } else if (auto member = method_type->to<IR::Member>()) {
        auto member_struct = get_member_struct(state, this, member);
        // try to resolve and find a function pointer
        if (member_struct.has_stack) {
            resolve_stack_call(this, state, member_struct, arguments);
            return false;
        }
        auto resolved_call =
            resolve_var_or_decl_parent(state, member_struct, arg_size);
        if (auto function = resolved_call->to<FunctionWrapper>()) {
            // call the function directly for now
            function->function_call(this, arguments);
            return false;
        } else if (auto decl = resolved_call->to<P4Declaration>()) {
            // We are retrieving a method from an extern object
            callable = decl->decl;
        } else {
            BUG("Unexpected method call member %s ",
                resolved_call->get_static_type());
        }
    } else {
        P4C_UNIMPLEMENTED("Method call %s not supported.", mce);
    }
    // at this point, we assume we are dealing with a Declaration
    params = get_params(callable);

    state->copy_in(this, params, arguments);
    visit(callable);
    state->copy_out();
    return false;
}

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    const IR::ParameterList *params;
    auto arguments = cce->arguments;
    if (auto c = resolved_type->to<IR::P4Control>()) {
        params = c->getApplyParameters();
    } else if (auto p = resolved_type->to<IR::P4Parser>()) {
        params = p->getApplyParameters();
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                          resolved_type, resolved_type->node_type_name());
    }
    state->copy_in(this, params, arguments);
    visit(resolved_type);
    state->copy_out();
    return false;
}

} // namespace TOZ3_V2
