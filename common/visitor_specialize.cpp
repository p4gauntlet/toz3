#include "visitor_specialize.h"

#include <cstddef>

#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "toz3/common/state.h"

namespace P4::ToZ3 {

const IR::Node *TypeModifier::postorder(IR::Type *type) {
    if (const auto *tn = type->to<IR::Type_Name>()) {
        if (type_mapping->count(tn->path->name.name) > 0) {
            return type_mapping->at(tn->path->name.name);
        }
    }
    if (const auto *tv = type->to<IR::Type_Var>()) {
        if (type_mapping->count(tv->name.name) > 0) {
            return type_mapping->at(tv->name.name);
        }
    }
    return type;
}

const IR::Node *TypeSpecializer::specialize(const IR::Node *node,
                                          const IR::TypeParameters *parameters) {
    BUG_CHECK(types.size() <= parameters->parameters.size(),
              "Too many type arguments when specializing %1%", node);
    std::map<cstring, const IR::Type *> type_mapping;
    for (size_t idx = 0; idx < types.size(); ++idx) {
        type_mapping.emplace(parameters->parameters.at(idx)->name.name,
                             state.resolve_type(types.at(idx)));
    }
    prune();
    return node->apply(TypeModifier(&type_mapping));
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Extern *te) {
    return specialize(te, te->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Package *tp) {
    return specialize(tp, tp->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::P4Control *c) {
    return specialize(c, c->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::P4Parser *p) {
    return specialize(p, p->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Control *tc) {
    return specialize(tc, tc->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::Type_Parser *tp) {
    return specialize(tp, tp->getTypeParameters());
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
    return specialize(ts, ts->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::Method *m) {
    return specialize(m, m->type->getTypeParameters());
}

const IR::Node *TypeSpecializer::preorder(IR::P4Action *a) {
    prune();
    return a;
}

const IR::Node *TypeSpecializer::preorder(IR::Function *f) {
    return specialize(f, f->type->getTypeParameters());
}

}  // namespace P4::ToZ3
