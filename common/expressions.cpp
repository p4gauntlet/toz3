#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/variant/get.hpp>

#include "../contrib/z3/z3++.h"
#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/vector.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/exceptions.h"
#include "lib/stringify.h"
#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"
#include "toz3/common/type_simple.h"
#include "type_base.h"
#include "util.h"
#include "visitor_interpret.h"
#include "visitor_specialize.h"

namespace TOZ3 {

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (const auto *tb = c->type->to<IR::Type_Bits>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = state->get_z3_ctx()->bv_val(val_string, tb->size);
        auto *wrapper = new Z3Bitvector(state, tb, expr, tb->isSigned);
        state->set_expr_result(wrapper);
        return false;
    }
    if (c->type->is<IR::Type_InfInt>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = state->get_z3_ctx()->int_val(val_string);
        auto *var = new Z3Int(state, expr);
        state->set_expr_result(var);
        return false;
    }
    P4C_UNIMPLEMENTED("Constant of type %s not implemented!", c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::BoolLiteral *bl) {
    auto expr = state->get_z3_ctx()->bool_val(bl->value);
    auto *wrapper = new Z3Bitvector(state, &BOOL_TYPE, expr);
    state->set_expr_result(wrapper);
    return false;
}

bool Z3Visitor::preorder(const IR::StringLiteral *sl) {
    auto expr = state->get_z3_ctx()->string_val(sl->value);
    auto *wrapper = new Z3Bitvector(state, &STRING_TYPE, expr);
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
    std::map<cstring, P4Z3Instance *> members;
    for (const auto *component : se->components) {
        visit(component);
        members[component->name] = state->copy_expr_result();
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
    state->set_expr_result(state->get_var(t->typeName->checkedTo<IR::Type_Name>()->path->name));
    return false;
}

/***
===============================================================================
MethodCallExpression
===============================================================================
***/

FunOrMethod get_function(const P4Z3Instance *parent_class, cstring member_identifier) {
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

void resolve_stack_call(Visitor *visitor, P4State *state, const MemberStruct &member_struct,
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
            if (const auto *function = boost::get<P4Z3Function>(&resolved_call)) {
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

FunOrMethod resolve_var_or_decl_parent(P4State *state, const MemberStruct &member_struct,
                                       int num_args) {
    const P4Z3Instance *parent_class = nullptr;
    if (const auto *decl = state->find_static_decl(member_struct.main_member)) {
        parent_class = decl;
    } else {
        // try to find the result in vars and fail otherwise
        parent_class = state->get_var(member_struct.main_member);
    }

    for (auto it = member_struct.mid_members.rbegin(); it != member_struct.mid_members.rend();
         ++it) {
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
    P4C_UNIMPLEMENTED("Callable declaration %s of type %s not supported.", callable,
                      callable->node_type_name());
}

P4Z3Instance *exec_function(Z3Visitor *visitor, const IR::Function *f) {
    auto *state = visitor->get_state();
    visitor->visit(f->body);

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
        return merged_return;
    }
    // If there are no return expressions, return a void result
    return new VoidResult();
}

P4Z3Instance *exec_method(Z3Visitor *visitor, const IR::Method *m) {
    auto *state = visitor->get_state();
    auto method_name = infer_name(m->getAnnotations(), m->name.name);
    const auto *method_type = state->resolve_type(m->type->returnType);
    // TODO: Different types of arguments and multiple calls
    for (const auto *param : *m->getParameters()) {
        cstring param_name = param->name.name;
        cstring merged_param_name = method_name + "_" + param_name;
        if (param->direction == IR::Direction::Out || param->direction == IR::Direction::InOut) {
            auto *instance = state->gen_instance(merged_param_name, param->type, 0);
            // TODO: Clean up, this should not be necessary
            if (auto *si = instance->to_mut<StructBase>()) {
                si->propagate_validity(nullptr);
                si->bind(nullptr, 0);
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
        si->propagate_validity(nullptr);
        si->bind(nullptr, 0);
    }
    return return_instance;
}

P4Z3Instance *exec_action(Z3Visitor *visitor, const IR::P4Action *a) {
    visitor->visit(a->body);
    return new VoidResult();
}

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    const IR::Node *callable = nullptr;
    const auto *arguments = mce->arguments;
    auto arg_size = arguments->size();

    const auto *method_type = mce->method;
    if (const auto *path_expr = method_type->to<IR::PathExpression>()) {
        // FIXME: This is a very rough version of overloading...
        auto path_identifier = path_expr->path->name.name + std::to_string(arg_size);
        callable = state->get_static_decl(path_identifier)->get_decl();
    } else if (const auto *member = method_type->to<IR::Member>()) {
        auto member_struct = get_member_struct(state, this, member);
        // try to resolve and find a function pointer
        if (member_struct.has_stack) {
            resolve_stack_call(this, state, member_struct, arguments);
            return false;
        }
        auto resolved_call = resolve_var_or_decl_parent(state, member_struct, arg_size);
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
    // At this point, we assume we are dealing with a declaration
    TypeSpecializer specializer(*state, *mce->typeArguments);
    callable = callable->clone()->apply(specializer);

    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    set_params(callable, &params, &type_params);

    const ParamInfo param_info = {*params, *arguments, *type_params, *mce->typeArguments};

    // Now we set all the inputs we have mapped.
    state->copy_in(this, param_info);
    // Switch based on the dynamic callable type. The visitor is too cumbersome.
    P4Z3Instance *return_expr = nullptr;
    if (const auto *a = callable->to<IR::P4Action>()) {
        return_expr = exec_action(this, a);
    } else if (const auto *a = callable->to<IR::Function>()) {
        return_expr = exec_function(this, a);
    } else if (const auto *a = callable->to<IR::Method>()) {
        return_expr = exec_method(this, a);
    } else {
        P4C_UNIMPLEMENTED("Can not call callable %s.", callable->node_type_name());
    }
    // Set the result.
    state->set_expr_result(return_expr);
    // Copy back the inputs that matter.
    state->copy_out();
    return false;
}

/***
===============================================================================
ConstructorCallExpression
===============================================================================
***/

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    const auto *arguments = cce->arguments;
    if (const auto *c = resolved_type->to<IR::P4Control>()) {
        params = c->getConstructorParameters();
        type_params = c->getTypeParameters();
    } else if (const auto *p = resolved_type->to<IR::P4Parser>()) {
        params = p->getConstructorParameters();
        type_params = p->getTypeParameters();
    } else if (const auto *ext = resolved_type->to<IR::Type_Extern>()) {
        // TODO: How to cleanly resolve this?
        // params = new IR::ParameterList();
        // const auto *ext_const = ext->lookupConstructor(arguments);
        auto *ext_instance = state->gen_instance(UNDEF_LABEL, ext);
        state->set_expr_result(ext_instance);
        return false;
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.", resolved_type,
                          resolved_type->node_type_name());
    }
    auto var_map = state->merge_args_with_params(this, *arguments, *params, *type_params);
    state->set_expr_result(new ControlInstance(state, resolved_type, var_map.second));
    return false;
}
}  // namespace TOZ3
