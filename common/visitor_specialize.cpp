#include "visitor_specialize.h"

#include <cstddef>

#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "toz3/common/state.h"

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
    prune();
    auto *resolved_type = state.get_type(tn->path->name)->clone();
    return apply_visitor(resolved_type);
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Var *tv) {
    prune();
    auto *resolved_type = state.get_type(tv->name.name)->clone();
    return apply_visitor(resolved_type);
}

const IR::Node *TypeSpecializer::preorder(IR::Type_StructLike *ts) {
    prune();
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = ts->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    return ts->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::Method *m) {
    prune();
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = m->type->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    return m->apply(type_modifier);
}

const IR::Node *TypeSpecializer::preorder(IR::P4Action *a) {
    prune();
    return a;
}

const IR::Node *TypeSpecializer::preorder(IR::Function *f) {
    prune();
    std::map<cstring, const IR::Type *> type_mapping;
    auto type_args_size = types.size();
    for (size_t idx = 0; idx < type_args_size; ++idx) {
        const auto *matched_type = state.resolve_type(types.at(idx));
        const auto *type_param = f->type->getTypeParameters()->parameters.at(idx);
        type_mapping.insert({type_param->getName().name, matched_type});
    }
    TypeModifier type_modifier(&type_mapping);
    return f->apply(type_modifier);
}

}  // namespace TOZ3
