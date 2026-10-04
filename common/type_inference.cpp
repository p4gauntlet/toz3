#include "type_inference.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "exceptions.h"
#include "state.h"
#include "visitor_specialize.h"

namespace P4::ToZ3 {
namespace {

cstring type_variable(const IR::Type *type) {
    if (const auto *name = type->to<IR::Type_Name>()) return name->path->name;
    if (const auto *variable = type->to<IR::Type_Var>()) return variable->name;
    return nullptr;
}

class FindTypeVariables : public Inspector {
    const IR::TypeParameters &parameters;
    bool preorder(const IR::TypeParameters *) override { return false; }
    bool preorder(const IR::Type *type) override {
        const auto name = type_variable(type);
        found |= name && parameters.getDeclByName(name);
        return !found;
    }

 public:
    bool found = false;
    explicit FindTypeVariables(const IR::TypeParameters &parameters) : parameters(parameters) {}
};

class CollectTypeConstraints {
    const P4State &state;
    const IR::TypeParameters &parameters;

 public:
    std::vector<std::pair<cstring, const IR::Type *>> bindings;
    CollectTypeConstraints(const P4State &state, const IR::TypeParameters &parameters)
        : state(state), parameters(parameters) {}

    void collect(const IR::Type *formal, const IR::Type *actual) {
        if (!actual || actual->is<IR::Type_Dontcare>() || actual->is<IR::Type_Unknown>()) return;
        const auto name = type_variable(formal);
        if (name && parameters.getDeclByName(name)) {
            bindings.emplace_back(name, architecture_type(state.resolve_type(actual)));
            return;
        }
        if (!contains_type_variables(formal, parameters)) return;

        // Specialize the prototype structurally. Resolving it through P4State
        // would try to look up the very variables this pass is inferring.
        if (const auto *specialized = formal->to<IR::Type_Specialized>()) {
            const auto *prototype = state.resolve_type(specialized->baseType);
            const auto *generic =
                prototype->checkedTo<IR::IMayBeGenericType>()->getTypeParameters();
            TypeBindings substitution;
            BUG_CHECK(specialized->arguments->size() <= generic->parameters.size(),
                      "Too many type arguments for %s", prototype);
            for (size_t i = 0; i < specialized->arguments->size(); ++i)
                substitution.emplace(generic->parameters.at(i)->name,
                                     specialized->arguments->at(i));
            collect(prototype->apply(TypeModifier(&substitution))->checkedTo<IR::Type>(), actual);
            return;
        }

        actual = architecture_type(state.resolve_type(actual));
        formal = architecture_type(formal);
        if (const auto *array = formal->to<IR::Type_Array>()) {
            if (const auto *argument = actual->to<IR::Type_Array>())
                collect(array->elementType, argument->elementType);
        } else if (const auto *tuple = formal->to<IR::Type_BaseList>()) {
            if (const auto *argument = actual->to<IR::Type_BaseList>()) {
                for (size_t i = 0;
                     i < std::min(tuple->components.size(), argument->components.size()); ++i)
                    collect(tuple->components.at(i), argument->components.at(i));
            }
        } else if (const auto *structure = formal->to<IR::Type_StructLike>()) {
            if (const auto *argument = actual->to<IR::Type_StructLike>()) {
                for (const auto *field : structure->fields) {
                    const auto *other =
                        argument->fields.getDeclaration<IR::StructField>(field->name);
                    if (other) collect(field->type, other->type);
                }
            }
        } else if (const auto *block = formal->to<IR::IApply>()) {
            const auto *argument = actual->to<IR::IApply>();
            if (!argument || formal->node_type_name() != actual->node_type_name()) return;
            const auto *formals = block->getApplyParameters();
            const auto *actuals = argument->getApplyParameters();
            for (size_t i = 0; i < std::min(formals->size(), actuals->size()); ++i)
                collect(formals->getParameter(i)->type, actuals->getParameter(i)->type);
        }
    }
};

}  // namespace

bool contains_type_variables(const IR::Type *type, const IR::TypeParameters &parameters) {
    if (parameters.empty()) return false;
    FindTypeVariables visitor(parameters);
    type->apply(visitor);
    return visitor.found;
}

const IR::Type *architecture_type(const IR::Type *type) {
    if (const auto *container = type->to<IR::IContainer>()) return container->getType();
    return type;
}

TypeBindings infer_type_arguments(const P4State &state, const IR::TypeParameters &parameters,
                                  const std::vector<TypeConstraint> &constraints) {
    CollectTypeConstraints collector(state, parameters);
    for (const auto &[formal, actual] : constraints) collector.collect(formal, actual);
    TypeBindings result;
    for (const auto &[name, type] : collector.bindings) {
        const auto [entry, inserted] = result.emplace(name, type);
        if (inserted || entry->second->equiv(*type)) continue;
        // Unsized integer arguments can acquire a bit width from another argument.
        // Two explicitly sized types must agree; argument order cannot choose a winner.
        if (entry->second->is<IR::Type_InfInt>() && type->is<IR::Type_Bits>()) {
            entry->second = type;
        } else if (!(type->is<IR::Type_InfInt>() && entry->second->is<IR::Type_Bits>())) {
            throw UnsupportedFeatureError("Conflicting type arguments for " + name.string() + ": " +
                                          entry->second->toString().string() + " and " +
                                          type->toString().string());
        }
    }
    return result;
}

bool arguments_match(const IR::ParameterList &parameters, const IR::Vector<IR::Argument> &arguments,
                     bool action) {
    if (arguments.size() > parameters.size()) return false;
    std::set<const IR::Parameter *> supplied;
    for (size_t i = 0; i < arguments.size(); ++i) {
        const auto *argument = arguments.at(i);
        const auto *parameter =
            argument->name ? parameters.getParameter(argument->name) : parameters.getParameter(i);
        if (!parameter || !supplied.insert(parameter).second) return false;
    }
    for (const auto *parameter : parameters) {
        if (!supplied.count(parameter) && !parameter->isOptional() && !parameter->defaultValue &&
            !(action && parameter->direction == IR::Direction::None))
            return false;
    }
    return true;
}

const IR::Type *expression_type(const P4State &state, const IR::Expression *expression) {
    if (const auto *constructor = expression->to<IR::ConstructorCallExpression>()) {
        return state.resolve_type(constructor->constructedType);
    }
    if (const auto *name = expression->to<IR::TypeNameExpression>()) {
        return state.resolve_type(name->typeName);
    }
    if (const auto *path = expression->to<IR::PathExpression>()) {
        if (const auto *declaration = state.find_static_decl(path->path->name)) {
            if (const auto *instance = declaration->get_decl()->to<IR::Declaration_Instance>())
                return state.resolve_type(instance->type);
        }
        return state.resolve_type(state.get_var_type(path->path->name));
    }
    if (const auto *cast = expression->to<IR::Cast>()) return state.resolve_type(cast->destType);
    if (const auto *call = expression->to<IR::MethodCallExpression>()) {
        if (const auto *path = call->method->to<IR::PathExpression>()) {
            const auto name = mangle_name(path->path->name, call->arguments->size());
            const auto *callable = state.resolve_callable(name, *call->arguments);
            if (const auto *function = callable->to<IR::Function>())
                return state.resolve_type(function->type->returnType);
            if (const auto *method = callable->to<IR::Method>())
                return state.resolve_type(method->type->returnType);
        }
    }
    if (const auto *index = expression->to<IR::ArrayIndex>()) {
        const auto *base = expression_type(state, index->left);
        if (const auto *array = base->to<IR::Type_Array>()) {
            return state.resolve_type(array->elementType);
        }
        if (const auto *tuple = base->to<IR::Type_BaseList>()) {
            const auto *constant = index->right->checkedTo<IR::Constant>();
            return state.resolve_type(tuple->components.at(constant->asUnsigned()));
        }
    }
    if (const auto *member = expression->to<IR::Member>()) {
        const auto *base = expression_type(state, member->expr);
        if (const auto *aggregate = base->to<IR::Type_StructLike>()) {
            const auto *field = aggregate->fields.getDeclaration<IR::StructField>(member->member);
            CHECK_NULL(field);
            return state.resolve_type(field->type);
        }
    }
    if (expression->type && !expression->type->is<IR::Type_Unknown>()) {
        return state.resolve_type(expression->type);
    }
    throw UnsupportedFeatureError("Cannot infer the type of " + expression->toString());
}

big_int serialized_size(const P4State &state, const IR::Type *type, bool maximum) {
    type = state.resolve_type(type);
    if (const auto *bits = type->to<IR::Type_Bits>()) return bits->size;
    if (const auto *bits = type->to<IR::Type_Varbits>()) return maximum ? bits->size : 0;
    if (type->is<IR::Type_Boolean>()) return 1;
    if (const auto *enumeration = type->to<IR::Type_SerEnum>()) {
        return serialized_size(state, enumeration->type, maximum);
    }
    if (const auto *array = type->to<IR::Type_Array>()) {
        return array->getSize() * serialized_size(state, array->elementType, maximum);
    }
    big_int size = 0;
    if (const auto *aggregate = type->to<IR::Type_StructLike>()) {
        for (const auto *field : aggregate->fields) {
            const auto field_size = serialized_size(state, field->type, maximum);
            if (type->is<IR::Type_HeaderUnion>())
                size = std::max(size, field_size);
            else
                size += field_size;
        }
        return size;
    }
    if (const auto *tuple = type->to<IR::Type_BaseList>()) {
        for (const auto *component : tuple->components) {
            size += serialized_size(state, component, maximum);
        }
        return size;
    }
    throw UnsupportedFeatureError("Size is undefined for type " + type->toString());
}
}  // namespace P4::ToZ3
