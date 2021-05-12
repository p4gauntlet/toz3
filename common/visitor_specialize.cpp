#include "visitor_specialize.h"
#include <cstddef>
#include <cstdio>

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
    TypeModifier type_modifier(&type_mapping);
    prune();
    return te->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Package *tp) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = tp->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    prune();
    return tp->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::P4Control *c) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = c->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    prune();
    return c->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::P4Parser *p) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = p->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    prune();
    return p->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Control *tc) {
    prune();
    return tc;
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Parser *tp) {
    prune();
    return tp;
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
