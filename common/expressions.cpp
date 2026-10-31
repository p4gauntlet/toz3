#include <z3++.h>

#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "defaults.h"
#include "exceptions.h"
#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/vector.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/exceptions.h"
#include "lib/stringify.h"
#include "packet.h"
#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"
#include "toz3/common/type_simple.h"
#include "type_base.h"
#include "type_inference.h"
#include "util.h"
#include "visitor_interpret.h"
#include "visitor_specialize.h"

namespace P4::ToZ3 {

bool Z3Visitor::preorder(const IR::Constant *c) {
    if (const auto *tb = c->type->to<IR::Type_Bits>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = tb->size == 0 ? state->get_z3_ctx()->int_val(0)
                                  : state->get_z3_ctx()->bv_val(val_string.c_str(), tb->size);
        auto *wrapper = allocate_instance<Z3Bitvector>(state, state, tb, expr, tb->isSigned);
        state->set_expr_result(wrapper);
        return false;
    }
    if (c->type->is<IR::Type_InfInt>()) {
        auto val_string = Util::toString(c->value, 0, false);
        auto expr = state->get_z3_ctx()->int_val(val_string.c_str());
        auto *var = allocate_instance<Z3Int>(state, state, expr);
        state->set_expr_result(var);
        return false;
    }
    throw UnsupportedFeatureError("Constant of type " + c->type->node_type_name() +
                                  " not implemented!");
}

bool Z3Visitor::preorder(const IR::BoolLiteral *bl) {
    auto expr = state->get_z3_ctx()->bool_val(bl->value);
    auto *wrapper = allocate_instance<Z3Bitvector>(state, state, &BOOL_TYPE, expr);
    state->set_expr_result(wrapper);
    return false;
}

bool Z3Visitor::preorder(const IR::StringLiteral *sl) {
    auto expr = state->get_z3_ctx()->string_val(sl->value.c_str());
    auto *wrapper = allocate_instance<Z3Bitvector>(state, state, &STRING_TYPE, expr);
    state->set_expr_result(wrapper);
    return false;
}

bool Z3Visitor::preorder(const IR::NamedExpression *ne) {
    // TODO: Figure out what the implications of a name are here...
    visit(ne->expression);
    return false;
}

bool Z3Visitor::preorder(const IR::Dots * /*unused*/) {
    state->set_expr_result(allocate_instance<DefaultInstance>(state, state));
    return false;
}

bool Z3Visitor::preorder(const IR::ListExpression *le) {
    std::vector<P4Z3Instance *> members;
    for (const auto &component : le->components) {
        visit(component);
        members.push_back(state->copy_expr_result());
    }
    state->set_expr_result(allocate_instance<ListInstance>(state, state, members, le->type));
    return false;
}

bool Z3Visitor::preorder(const IR::StructExpression *se) {
    std::map<cstring, P4Z3Instance *> members;
    for (const auto &component : se->components) {
        visit(component);
        members[component->name] = state->copy_expr_result();
    }
    // TODO: Not sure what the deal with Type_Unknown is here
    if (se->type != nullptr && !se->type->is<IR::Type_Unknown>()) {
        auto *instance = state->gen_instance(cstring(UNDEF_LABEL), se->type);
        if (auto *struct_instance = instance->to_mut<StructBase>()) {
            struct_instance->set_list(members);
        } else {
            throw UnsupportedFeatureError("Unsupported StructExpression class " + se->toString());
        }
        state->set_expr_result(instance);
    } else {
        state->set_expr_result(allocate_instance<ListInstance>(state, state, members, se->type));
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
    throw UnsupportedFeatureError("Retrieving a function not implemented for type " +
                                  parent_class->get_static_type());
}

void resolve_stack_call(Z3Visitor *visitor, P4State *state, const MemberStruct &member_struct,
                        const IR::Vector<IR::Argument> *arguments) {
    auto arg_size = arguments->size();
    auto parents = get_member_parents(state, member_struct);
    P4Z3Instance *result = nullptr;

    // Execute and merge the functions
    const auto *name = std::get_if<cstring>(&member_struct.members.back());
    if (name == nullptr) {
        throw UnsupportedFeatureError("Member type not implemented.");
    }
    for (auto &parent_pair : parents) {
        auto cond = parent_pair.first;
        auto *parent_class = parent_pair.second;
        auto member_identifier = mangle_name(*name, arg_size);
        auto resolved_call = get_function(parent_class, member_identifier);
        if (const auto *function = std::get_if<P4Z3Function>(&resolved_call)) {
            // A union member's validity method also changes its siblings.
            auto *changed = parent_class;
            if (auto *header = parent_class->to_mut<HeaderInstance>()) {
                if (header->get_parent_union() != nullptr) {
                    changed = header->get_parent_union();
                }
            }
            const auto *orig_class = changed->copy();
            (*function)(visitor, arguments);
            const auto *returned = state->get_expr_result();
            if (!returned->is<VoidResult>()) {
                if (result == nullptr) {
                    result = state->gen_instance(cstring(UNDEF_LABEL), returned->get_p4_type());
                }
                result->merge(cond, *returned);
            }
            changed->merge(!cond, *orig_class);
        } else {
            throw InternalError("Unexpected stack call member");
        }
    }
    state->set_expr_result(result != nullptr ? result : allocate_instance<VoidResult>(state));
}

FunOrMethod resolve_var_or_decl_parent(P4State *state, const MemberStruct &member_struct,
                                       const IR::Vector<IR::Argument> &arguments) {
    const auto *parent_class = get_member_parent(state, member_struct);
    const auto *name = std::get_if<cstring>(&member_struct.members.back());
    if (name == nullptr) {
        throw UnsupportedFeatureError("Member type not implemented.");
    }
    if (parent_class->is<ExternInstance>()) {
        const auto *type = parent_class->get_p4_type()->checkedTo<IR::Type_Extern>();
        const IR::Method *selected = nullptr;
        for (const auto &method : type->methods) {
            if (method->name != *name || !arguments_match(*method->getParameters(), arguments)) {
                continue;
            }
            BUG_CHECK(!selected, "Ambiguous extern method %s", name);
            selected = method;
        }
        BUG_CHECK(selected, "No matching extern method %s", name);
        return selected;
    }
    auto member_identifier = mangle_name(*name, arguments.size());
    return get_function(parent_class, member_identifier);
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
    throw UnsupportedFeatureError("Callable declaration " + callable->toString() + " of type " +
                                  callable->node_type_name() + " not supported.");
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
        const auto return_type = f->type->returnType;
        auto *merged_return = begin->second->cast_allocate(return_type);
        for (auto it = std::next(begin); it != end; ++it) {
            z3::expr cond = it->first;
            const auto *then_var = it->second->cast_allocate(return_type);
            merged_return->merge(cond, *then_var);
        }
        return merged_return;
    }
    // If there are no return expressions, return a void result
    return allocate_instance<VoidResult>(state);
}

P4Z3Instance *exec_method(Z3Visitor *visitor, const IR::Method *m, bool packet_in) {
    auto *state = visitor->get_state();
    packet_in = packet_in && (state->find_var("$packet_id"_cs) != nullptr);
    if (packet_in) {
        return execute_packet_method(visitor, m);
    }
    if (m->name == "verify" && (state->find_var("$parser_error"_cs) != nullptr)) {
        const auto *condition =
            state->get_var(m->getParameters()->getParameter(0)->name)->to<NumericVal>();
        const auto *error =
            state->get_var(m->getParameters()->getParameter(1)->name)->to<EnumBase>();
        CHECK_NULL(condition);
        CHECK_NULL(error);
        visitor->reject_parser(!*condition->get_val(), *error->get_val());
        return allocate_instance<VoidResult>(state);
    }
    auto method_name = infer_name(m, m->name.name);
    const auto method_type = state->resolve_type(m->type->returnType);
    // TODO: Different types of arguments and multiple calls
    for (const auto &param : *m->getParameters()) {
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
    return allocate_instance<VoidResult>(visitor->get_state());
}

namespace {
bool evaluateSizeMethod(P4State *state, const IR::MethodCallExpression *mce) {
    if (const auto *member = mce->method->to<IR::Member>()) {
        const auto name = member->member.name;
        if (name == "minSizeInBits" || name == "minSizeInBytes" || name == "maxSizeInBits" ||
            name == "maxSizeInBytes") {
            BUG_CHECK(mce->arguments->empty(), "Size methods take no arguments");
            const auto type = expression_type(*state, member->expr);
            auto size =
                serialized_size(*state, type, name == "maxSizeInBits" || name == "maxSizeInBytes");
            if (name == "minSizeInBytes" || name == "maxSizeInBytes") {
                size = (size + CHAR_BIT - 1) / CHAR_BIT;
            }
            state->set_expr_result(allocate_instance<Z3Int>(
                state, state,
                state->get_z3_ctx()->int_val(Util::toString(size, 0, false).c_str())));
            return true;
        }
    }
    return false;
}
}  // namespace

bool Z3Visitor::preorder(const IR::MethodCallExpression *mce) {
    if (evaluateSizeMethod(state, mce)) {
        return false;
    }
    const IR::Node *callable = nullptr;
    bool packet_in = false;
    const auto arguments = mce->arguments;
    auto arg_size = arguments->size();

    const auto method_type = mce->method;
    if (const auto *path_expr = method_type->to<IR::PathExpression>()) {
        auto path_identifier = mangle_name(path_expr->path->name.name, arg_size);
        callable = state->resolve_callable(path_identifier, *arguments);
    } else if (const auto *member = method_type->to<IR::Member>()) {
        auto member_struct = get_member_struct(state, this, member);
        if (state->find_var(member_struct.main_member) != nullptr) {
            const auto parents = get_member_parents(state, member_struct);
            if (parents.size() == 1) {
                const auto *type = parents.front().second->get_p4_type()->to<IR::Type_Extern>();
                packet_in = (type != nullptr) && type->name == "packet_in";
            }
        }
        // try to resolve and find a function pointer
        if (member_struct.has_index() &&
            !get_member_parent(state, member_struct)->is<ExternInstance>()) {
            resolve_stack_call(this, state, member_struct, arguments);
            return false;
        }
        // Extern array elements share the scalar extern-call abstraction. Resolve
        // the selected handle, then evaluate arguments/copy-out once, including
        // when the saved receiver index is symbolic.
        auto resolved_call = resolve_var_or_decl_parent(state, member_struct, *arguments);
        if (const auto *function = std::get_if<P4Z3Function>(&resolved_call)) {
            // call the function directly for now
            (*function)(this, arguments);
            return false;
        }
        if (const auto *decl = std::get_if<const IR::Method *>(&resolved_call)) {
            // We are retrieving a method from an extern object
            callable = *decl;
        } else {
            throw InternalError("Unexpected method call member.");
        }
    } else {
        throw UnsupportedFeatureError("Method call " + mce->toString() + " not supported.");
    }
    // At this point, we assume we are dealing with a declaration
    executeCallable(mce, callable, packet_in);
    return false;
}

void Z3Visitor::executeCallable(const IR::MethodCallExpression *mce, const IR::Node *callable,
                                bool packet_in) {
    const auto arguments = mce->arguments;
    TypeSpecializer specializer(*state, *mce->typeArguments);
    auto specialized = callable->apply(specializer);
    callable = specialized;

    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    set_params(callable, &params, &type_params);

    const ParamInfo param_info = {*params, *arguments, *type_params, *mce->typeArguments};

    // Now we set all the inputs we have mapped.
    const auto inferred_types = state->copy_in(this, param_info);
    if (state->has_exited()) {
        state->pop_scope();
        state->set_expr_result(allocate_instance<VoidResult>(state));
        return;
    }
    // Substitute into the declaration rather than shadowing caller-side type names.
    specialized = callable->apply(TypeModifier(&inferred_types));
    callable = specialized;
    // Switch based on the dynamic callable type. The visitor is too cumbersome.
    P4Z3Instance *return_expr = nullptr;
    if (const auto *a = callable->to<IR::P4Action>()) {
        return_expr = exec_action(this, a);
    } else if (const auto *a = callable->to<IR::Function>()) {
        return_expr = exec_function(this, a);
    } else if (const auto *a = callable->to<IR::Method>()) {
        return_expr = exec_method(this, a, packet_in);
    } else {
        throw UnsupportedFeatureError("Can not call callable " + callable->node_type_name());
    }
    // Set the result.
    state->set_expr_result(return_expr);
    // Copy back the inputs that matter.
    const auto saved_outputs = state->get_copy_out_args();
    state->copy_out();
    if (packet_in && !state->has_exited() && callable->checkedTo<IR::Method>()->name == "extract") {
        // The out destination was saved before any other arguments were evaluated.
        for (const auto &argument : saved_outputs) {
            if (argument.first.next_stack) {
                auto *stack =
                    get_member(state, *argument.first.next_stack)->to_mut<StackInstance>();
                CHECK_NULL(stack);
                stack->advance_next();
            }
        }
    }
}

/***
===============================================================================
ConstructorCallExpression
===============================================================================
***/

const IR::Type_Extern *Z3Visitor::specialize_extern(const IR::Type_Extern *type,
                                                    const IR::Vector<IR::Argument> &arguments) {
    if (type->getTypeParameters()->empty()) {
        return type;
    }
    const auto constructor = type->lookupConstructor(&arguments);
    CHECK_NULL(constructor);
    std::vector<TypeConstraint> constraints;
    for (size_t idx = 0; idx < arguments.size(); ++idx) {
        const auto argument = arguments.at(idx);
        const auto parameter = argument->name
                                   ? constructor->getParameters()->getParameter(argument->name.name)
                                   : constructor->getParameters()->getParameter(idx);
        if (!contains_type_variables(parameter->type, *type->getTypeParameters())) {
            continue;
        }
        visit(argument->expression);
        constraints.push_back({parameter->type, state->get_expr_result()->get_p4_type()});
    }
    const auto mapping = infer_type_arguments(*state, *type->getTypeParameters(), constraints);
    TypeModifier modifier(&mapping);
    return type->apply(modifier)->checkedTo<IR::Type_Extern>();
}

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    auto resolved_type = state->resolve_type(cce->constructedType);
    const auto arguments = cce->arguments;
    if (const auto *ext = resolved_type->to<IR::Type_Extern>()) {
        auto *ext_instance =
            state->gen_instance(cstring(UNDEF_LABEL), specialize_extern(ext, *arguments));
        state->set_expr_result(ext_instance);
        return false;
    }
    const auto *container = resolved_type->to<IR::IContainer>();
    if ((container == nullptr) || !resolved_type->is<IR::IApply>()) {
        throw UnsupportedFeatureError("Type Declaration " + resolved_type->toString() +
                                      " of type " + resolved_type->node_type_name() +
                                      " not supported.");
    }
    auto var_map = state->merge_args_with_params(
        this, *arguments, *container->getConstructorParameters(), *container->getTypeParameters());
    resolved_type = resolved_type->apply(TypeModifier(&var_map.types))->checkedTo<IR::Type>();
    state->set_expr_result(
        allocate_instance<ControlInstance>(state, state, resolved_type, var_map.values));
    return false;
}
}  // namespace P4::ToZ3
