#include "codegen.h"

namespace TOZ3_V2 {

Visitor::profile_t CodeGenToz3::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

void CodeGenToz3::end_apply(const IR::Node *) {}

bool CodeGenToz3::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (auto o : p->objects) {
        visit(o);
    }
    return false;
}

void CodeGenToz3::fill_with_z3_sorts(std::vector<const IR::Node *> *sorts,
                                     const IR::Type *t) {
    if (auto tn = t->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        if (type_map.count(type_name)) {
            const IR::Type *sub_type = type_map[type_name];
            sorts->push_back(sub_type);
        } else {
            BUG("Type name \"%s\" not found!.", type_name);
        }
    } else {
        sorts->push_back(t);
    }
}

bool CodeGenToz3::preorder(const IR::Type_StructLike *t) {
    type_map.emplace(t->name.name, t);
    return false;
}

bool CodeGenToz3::preorder(const IR::Type_Stack *) { return false; }
bool CodeGenToz3::preorder(const IR::Type_Enum *t) {
    type_map.emplace(t->name.name, t);
    return false;
}
bool CodeGenToz3::preorder(const IR::Type_Error *t) {
    type_map.emplace(t->name.name, t);
    return false;
}

} // namespace TOZ3_V2
