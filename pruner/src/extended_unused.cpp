#include "extended_unused.h"

#include "frontends/p4/sideEffects.h"
#include "pruner_util.h"

namespace P4PRUNER {

bool PruneUnused::check_if_field_used(cstring name_of_struct,
                                      cstring name_of_field) {
    INFO("current struct_used");
    // show_used_structs();    
    for (struct_obj *s : *used_structs) {
        if (s->name == name_of_struct) {
            for (cstring f : *(s->fields)) {
                if (f == name_of_field) {
                    return true;
                }
            }
        }
    }
    return false;
}

const IR::Node *PruneUnused::preorder(IR::Type_StructLike *ts) {
    // prune();  // do not remove individual struct members yet
    // INFO(ts);
    if (!unused_refMap->isUsed(getOriginal<IR::IDeclaration>())) {
        return nullptr;
    }
    return ts;
}

const IR::Node *PruneUnused::preorder(IR::StructField *sf) {
    auto p = getParent<IR::Type_StructLike>();
    cstring p_name = p->getP4Type()->toString();
    cstring f_name = sf->toString();
    bool res = check_if_field_used(p_name, f_name);
    if(res) return sf;
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
    IR::PathExpression *pexpr = (IR::PathExpression *)(p->expr);
    const IR::IDeclaration *decl =
        unused_refMap->getDeclaration(pexpr->path, false);
    if (decl == nullptr)
        return true;
    const IR::Parameter *v = decl->to<IR::Parameter>();
    if (v == nullptr)
        return true;  // Only handle parameters for now
    cstring mem = p->member.name;
    cstring parent = v->type->getP4Type()->toString();
    // INFO("Member: " << mem);
    // INFO("ParentStruct: " << parent);
    insertField(parent, mem);
    return true;
}
Visitor::profile_t ListStructs::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

void ListStructs::insertField(cstring name_of_struct, cstring name_of_field) {
    bool foundStruct = false;
    bool foundField = false;

    for (size_t i = 0; i < used_structs->size(); i++) {
        struct_obj* s = used_structs->at(i);
        if (name_of_struct == s->name) {
            foundStruct = true;
            INFO("Fields size : " << s->fields->size());
            for (size_t j = 0; j < s->fields->size(); j++) {
                cstring f = s->fields->at(j);
                INFO("Got " << f);
                if (name_of_field == f) {
                    foundField = true;
                }
            }
            if (!foundField)
                s->fields->push_back(name_of_field);
        }
    }
    if (!foundStruct) {
        // Lets have this on the heap
        std::vector<cstring> *f = new std::vector<cstring>();
        f->push_back(name_of_field);
        struct_obj* s = new struct_obj;
        s->name = name_of_struct;
        s->fields = f;
        used_structs->push_back(s);
    }
}

void PruneUnused::show_used_structs() {
    INFO("Printing Used Structs");
    if (used_structs->size() == 0) {
        INFO("used_structs is empty");
        return;
    }
    for (struct_obj *s : *used_structs) {
        INFO("Struct: " << s->name);
        for (cstring f : *(s->fields)) {
            INFO("\tField: " << f);
        }
    }
}

}  // namespace P4PRUNER
