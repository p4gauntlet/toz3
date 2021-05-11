#include "visitor_specialize.h"
#include <cstddef>
#include <cstdio>

#include "ir/ir-generated.h"
#include "lib/null.h"

namespace TOZ3 {

const IR::Node *TypeModifier::postorder(IR::Type *type) {
    if (const auto *tn = type->to<IR::Type_Name>()) {
        if (type_mapping->count(tn->path->name.name) > 0) {
            return type_mapping->at(tn->path->name.name);
        }
    }
    return type;
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Extern *te) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = te->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    IR::Vector<IR::Method> new_methods;
    // TODO: Massive mess. Clean up
    for (const auto *method : te->methods) {
        const auto *method_type = method->type;
        auto *matched_params = new IR::ParameterList();
        for (const auto *param : *method_type->parameters) {
            if (state.check_for_type(param->type) != nullptr) {
                matched_params->push_back(param);
                continue;
            }
            const auto *type_name = param->type->to<IR::Type_Name>();
            const auto *matched_type = type_mapping[type_name->path->name];
            if (matched_type == nullptr) {
                matched_params->push_back(param);
                continue;
            }
            auto *new_param =
                new IR::Parameter(param->name, param->direction, matched_type);
            matched_params->push_back(new_param);
        }
        const auto *return_type = method_type->returnType;
        if (return_type != nullptr) {
            if (state.check_for_type(return_type) == nullptr) {
                if (const auto *type_name = return_type->to<IR::Type_Name>()) {
                    const auto *matched_type =
                        type_mapping[type_name->path->name];
                    if (matched_type != nullptr) {
                        return_type = matched_type;
                    }
                }
            }
        }
        auto *new_method_type = method_type->clone();
        new_method_type->returnType = return_type;
        new_method_type->parameters = matched_params;
        auto *new_method = method->clone();
        new_method->type = new_method_type;
        new_methods.push_back(new_method);
    }
    te->methods = new_methods;
    prune();
    return te;
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Package *tp) {
    // TODO: Implement
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = tp->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(state, &type_mapping);
    prune();
    return tp->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::P4Control *tc) {
    std::map<cstring, const IR::Type *> param_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = tc->getTypeParameters()->parameters.at(idx);
        param_mapping.insert({type_param->getName().name, matched_type});
    }
    prune();
    return tc;
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Name *tn) {
    auto *resolved_type = state.get_type(tn->path->name)->clone();
    return resolved_type->apply_visitor_preorder(*this);
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Var *tv) {
    auto *resolved_type = state.get_type(tv->name.name)->clone();
    return resolved_type->apply_visitor_preorder(*this);
}

}  // namespace TOZ3
