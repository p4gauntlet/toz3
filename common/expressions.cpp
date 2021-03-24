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

bool Z3Visitor::preorder(const IR::TypeNameExpression *t) {
    state->set_expr_result(state->get_var(t->typeName->path->name));
    return false;
}

const P4Z3Instance *resolve_var_or_decl_parent(Z3Visitor *visitor,
                                               const IR::Member *m,
                                               int num_args) {
    const IR::Expression *parent = m->expr;
    const P4Z3Instance *complex_type;
    if (auto member = parent->to<IR::Member>()) {
        visitor->visit(member);
        complex_type = visitor->state->get_expr_result();
    } else if (auto path_expr = parent->to<IR::PathExpression>()) {
        P4Scope *scope;
        cstring name = path_expr->path->name.name;
        if (auto decl = visitor->state->find_static_decl(name, &scope)) {
            complex_type = decl;
        } else {
            // try to find the result in vars and fail otherwise
            complex_type = visitor->state->get_var(name);
        }
    } else if (auto method = parent->to<IR::MethodCallExpression>()) {
        visitor->visit(method);
        complex_type = visitor->state->get_expr_result();
    } else {
        P4C_UNIMPLEMENTED("Parent %s of type %s not implemented!", parent,
                          parent->node_type_name());
    }
    // FIXME: This is a very rough version of overloading...
    auto member_identifier = m->member.name + std::to_string(num_args);
    return complex_type->get_function(member_identifier);
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
        // try to resolve and find a function pointer
        auto resolved_call = resolve_var_or_decl_parent(this, member, arg_size);
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
    state->copy_out(this);
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
    state->copy_out(this);
    return false;
}

} // namespace TOZ3_V2
