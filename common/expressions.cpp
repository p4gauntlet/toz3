#include <cstdio>
#include <iostream>
#include <utility>

#include "../contrib/z3/z3++.h"
#include "lib/exceptions.h"

#include "util.h"
#include "visitor_interpret.h"

namespace TOZ3 {

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (const auto *tb = c->type->to<IR::Type_Bits>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = state->get_z3_ctx()->bv_val(val_string, tb->size);
        auto wrapper = Z3Bitvector(state, tb, expr, tb->isSigned);
        state->set_expr_result(wrapper);
        return false;
    }
    if (c->type->is<IR::Type_InfInt>()) {
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
    Z3Bitvector wrapper = Z3Bitvector(state, &BOOL_TYPE, expr);
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
    for (const auto *component : le->components) {
        visit(component);
        members.push_back(state->copy_expr_result());
    }
    state->set_expr_result(new ListInstance(state, members, le->type));
    return false;
}

bool Z3Visitor::preorder(const IR::StructExpression *se) {
    std::vector<P4Z3Instance *> members;
    for (const auto *component : se->components) {
        visit(component);
        members.push_back(state->copy_expr_result());
    }
    // TODO: Not sure what the deal with Type_Unknown is here
    if (se->type != nullptr && !se->type->is<IR::Type_Unknown>()) {
        auto *instance = state->gen_instance(UNDEF_LABEL, se->type);
        if (auto *struct_instance = instance->to_mut<StructBase>()) {
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

FunOrMethod get_function(const P4Z3Instance *parent_class,
                         cstring member_identifier) {
    // TODO: Think about how to merge these types. Traits?
    if (const auto *hdr = parent_class->to<HeaderInstance>()) {
        return hdr->get_function(member_identifier);
    }
    if (const auto *stack = parent_class->to<StackInstance>()) {
        return stack->get_function(member_identifier);
    }
    if (const auto *ext = parent_class->to<ExternInstance>()) {
        return ext->get_function(member_identifier);
    }
    if (const auto *decl = parent_class->to<ControlInstance>()) {
        return decl->get_function(member_identifier);
    }
    if (const auto *decl = parent_class->to<P4TableInstance>()) {
        return decl->get_function(member_identifier);
    }
    if (const auto *decl = parent_class->to<HeaderUnionInstance>()) {
        return decl->get_function(member_identifier);
    }
    P4C_UNIMPLEMENTED("Retrieving a function not implemented for type %s.",
                      parent_class->get_static_type());
}

void resolve_stack_call(Visitor *visitor, P4State *state,
                        const MemberStruct &member_struct,
                        const IR::Vector<IR::Argument> *arguments) {
    auto arg_size = arguments->size();
    auto hdr_pairs = get_hdr_pairs(state, member_struct);

    // Execute and merge the functions
    if (const auto *name = boost::get<cstring>(&member_struct.target_member)) {
        std::vector<std::pair<z3::expr, VarMap>> call_vars;
        for (auto &parent_pair : hdr_pairs) {
            auto cond = parent_pair.first;
            auto *parent_class = parent_pair.second;
            auto member_identifier = *name + std::to_string(arg_size);
            auto resolved_call = get_function(parent_class, member_identifier);
            if (const auto *function =
                    boost::get<P4Z3Function>(&resolved_call)) {
                // TODO: Support global side effects
                // For now, we only merge with the current class
                // There is some strange behavior here when using all state
                const auto *orig_class = parent_class->copy();
                (*function)(visitor, arguments);
                parent_class->merge(!cond, *orig_class);
            } else {
                BUG("Unexpected stack call member");
            }
        }
    } else {
        P4C_UNIMPLEMENTED("Member type not implemented.");
    }
}

FunOrMethod resolve_var_or_decl_parent(P4State *state,
                                       const MemberStruct &member_struct,
                                       int num_args) {
    const P4Z3Instance *parent_class = nullptr;
    if (const auto *decl = state->find_static_decl(member_struct.main_member)) {
        parent_class = decl;
    } else {
        // try to find the result in vars and fail otherwise
        parent_class = state->get_var(member_struct.main_member);
    }

    for (auto it = member_struct.mid_members.rbegin();
         it != member_struct.mid_members.rend(); ++it) {
        auto mid_member = *it;
        if (const auto *name = boost::get<cstring>(&mid_member)) {
            parent_class = parent_class->get_member(*name);
        } else {
            P4C_UNIMPLEMENTED("Member type not supported.");
        }
    }
    if (const auto *name = boost::get<cstring>(&member_struct.target_member)) {
        // FIXME: This is a very rough version of overloading...
        auto member_identifier = *name + std::to_string(num_args);
        return get_function(parent_class, member_identifier);
    }
    P4C_UNIMPLEMENTED("Member type not implemented.");
}

void set_params(const IR::Node *callable, const IR::ParameterList **params,
                const IR::TypeParameters **type_params) {
    if (const auto *p4action = callable->to<IR::P4Action>()) {
        *params = p4action->getParameters();
        *type_params = new IR::TypeParameters();
        return;
    }
    if (const auto *fun = callable->to<IR::Function>()) {
        *params = fun->getParameters();
        *type_params = fun->type->getTypeParameters();
        return;
    }
    if (const auto *method = callable->to<IR::Method>()) {
        *params = method->getParameters();
        *type_params = method->type->getTypeParameters();
        return;
    }
    P4C_UNIMPLEMENTED("Callable declaration %s of type %s not supported.",
                      callable, callable->node_type_name());
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Node *callable = nullptr;
    const auto *arguments = mce->arguments;
    auto arg_size = arguments->size();

    const auto *method_type = mce->method;
    if (const auto *path_expr = method_type->to<IR::PathExpression>()) {
        // FIXME: This is a very rough version of overloading...
        auto path_identifier =
            path_expr->path->name.name + std::to_string(arg_size);
        callable = state->get_static_decl(path_identifier)->decl;
    } else if (const auto *member = method_type->to<IR::Member>()) {
        auto member_struct = get_member_struct(state, this, member);
        // try to resolve and find a function pointer
        if (member_struct.has_stack) {
            resolve_stack_call(this, state, member_struct, arguments);
            return false;
        }
        auto resolved_call =
            resolve_var_or_decl_parent(state, member_struct, arg_size);
        if (const auto *function = boost::get<P4Z3Function>(&resolved_call)) {
            // call the function directly for now
            (*function)(this, arguments);
            return false;
        }
        if (auto *decl = boost::get<const IR::Method *>(&resolved_call)) {
            // We are retrieving a method from an extern object
            callable = *decl;
        } else {
            BUG("Unexpected method call member.");
        }
    } else {
        P4C_UNIMPLEMENTED("Method call %s not supported.", mce);
    }
    // at this point, we assume we are dealing with a Declaration
    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    set_params(callable, &params, &type_params);

    const ParamInfo param_info = {*params, *arguments, *type_params,
                                  *mce->typeArguments};
    state->copy_in(this, param_info);
    visit(callable);
    state->copy_out();
    return false;
}

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    const IR::ParameterList *params = nullptr;
    const auto *arguments = cce->arguments;
    // TODO: At this point we need to bind the types...
    if (const auto *c = resolved_type->to<IR::P4Control>()) {
        params = c->getApplyParameters();
    } else if (const auto *p = resolved_type->to<IR::P4Parser>()) {
        params = p->getApplyParameters();
    } else if (const auto *ext = resolved_type->to<IR::Type_Extern>()) {
        // TODO: How to cleanly resolve this?
        params = new IR::ParameterList();
        // const auto *ext_const = ext->lookupConstructor(arguments);
        auto *ext_instance = state->gen_instance(UNDEF_LABEL, ext);
        state->set_expr_result(ext_instance);
        return false;
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                          resolved_type, resolved_type->node_type_name());
    }
    const ParamInfo param_info = {*params, *arguments, {}, {}};
    state->copy_in(this, param_info);
    visit(resolved_type);
    state->copy_out();
    return false;
}

}  // namespace TOZ3
