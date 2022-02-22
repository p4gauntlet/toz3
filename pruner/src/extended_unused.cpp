#include "extended_unused.h"

#include "frontends/p4/sideEffects.h"
#include "pruner_util.h"

namespace P4PRUNER {

bool PruneUnused::check_if_field_used(cstring name_of_struct,
                                      cstring name_of_field) {
    for (const auto *s : *used_structs) {
        if (s->name == name_of_struct) {
            for (auto f : *(s->fields)) {
                if (f == name_of_field) {
                    return true;
                }
            }
        }
    }
    return false;
}

const IR::Node *PruneUnused::preorder(IR::Type_StructLike *ts) {
    if (!unused_refMap->isUsed(getOriginal<IR::IDeclaration>())) {
        return nullptr;
    }
    return ts;
}

const IR::Node *PruneUnused::preorder(IR::StructField *sf) {
    const auto *p = getParent<IR::Type_StructLike>();
    cstring p_name = p->getP4Type()->toString();
    cstring f_name = sf->toString();
    bool res = check_if_field_used(p_name, f_name);
    if (res) {
        return sf;
    }
    return nullptr;
}

const IR::Node *PruneUnused::preorder(IR::Type_Extern *te) {
    if (!unused_refMap->isUsed(getOriginal<IR::IDeclaration>())) {
        return nullptr;
    }
    return te;
}

const IR::Node *PruneUnused::preorder(IR::Method *m) {
    if (!unused_refMap->isUsed(getOriginal<IR::IDeclaration>())) {
        return nullptr;
    }
    return m;
}

const IR::Node *PruneUnused::preorder(IR::Function *f) {
    if (!unused_refMap->isUsed(getOriginal<IR::IDeclaration>())) {
        return nullptr;
    }
    return f;
}

// List unused struct declarations

bool ListStructs::preorder(const IR::Member *p) {
    const auto *pexpr = p->expr->to<IR::PathExpression>();
    if (pexpr == nullptr) {
        return true;  // i.e, expr was not a path expression
    }
    const IR::IDeclaration *decl =
        unused_refMap->getDeclaration(pexpr->path, false);
    if (decl == nullptr) {
        return true;
    }
    const auto *v = decl->to<IR::Parameter>();
    if (v == nullptr) {
        return true;  // Only handle parameters for now
    }
    cstring mem = p->member.name;
    cstring parent = v->type->getP4Type()->toString();
    insertField(parent, mem);
    return true;
}

Visitor::profile_t ListStructs::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

void ListStructs::insertField(cstring name_of_struct, cstring name_of_field) {
    bool foundStruct = false;
    bool foundField = false;

    for (auto *s : *used_structs) {
        if (name_of_struct == s->name) {
            foundStruct = true;
            for (auto f : *s->fields) {
                foundField = name_of_field == f;
            }
            if (!foundField) {
                s->fields->push_back(name_of_field);
            }
        }
    }
    if (!foundStruct) {
        // Lets have this on the heap
        auto *f = new std::vector<cstring>();
        f->push_back(name_of_field);
        auto *s = new struct_obj;
        s->name = name_of_struct;
        s->fields = f;
        used_structs->push_back(s);
    }
}

void PruneUnused::show_used_structs() {
    LOG2("Printing Used Structs");
    if (used_structs->empty()) {
        LOG1("used_structs is empty");
        return;
    }
    for (struct_obj *s : *used_structs) {
        LOG2("Struct: " << s->name);
        for (cstring f : *(s->fields)) {
            LOG2("\tField: " << f);
        }
    }
}

}  // namespace P4PRUNER
