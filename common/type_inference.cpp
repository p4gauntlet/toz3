#include "type_inference.h"

#include <algorithm>
#include <set>

#include "exceptions.h"
#include "state.h"

namespace P4::ToZ3 {
std::map<cstring, const IR::Type *> get_type_mapping(const IR::ParameterList *src_params,
                                                     const IR::TypeParameters *src_type_params,
                                                     const IR::ParameterList *dest_params) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto dest_params_size = dest_params->size();
    for (size_t idx = 0; idx < src_params->size(); ++idx) {
        // Ignore optional parameters.
        if (idx >= dest_params_size) {
            continue;
        }
        const auto *src_param = src_params->getParameter(idx);
        if (const auto *tn = src_param->type->to<IR::Type_Name>()) {
            auto src_type_name = tn->path->name.name;
            if (src_type_params->getDeclByName(src_type_name) != nullptr) {
                const auto *dst_param = dest_params->getParameter(idx);
                type_mapping.emplace(src_type_name, dst_param->type);
            }
        }
    }
    return type_mapping;
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
    if (const auto *name = expression->to<IR::TypeNameExpression>()) {
        return state.resolve_type(name->typeName);
    }
    if (const auto *path = expression->to<IR::PathExpression>()) {
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
