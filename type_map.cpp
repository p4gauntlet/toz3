#include <utility>

#include "type_map.h"
#include "complex_type.h"
#include "lib/exceptions.h"

namespace TOZ3_V2 {

Visitor::profile_t TypeVisitor::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

void TypeVisitor::end_apply(const IR::Node *) {}

bool TypeVisitor::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (auto o : p->objects) {
        visit(o);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Type_StructLike *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Enum *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Error *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Extern *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Package *t) {
    state->type_map[t->name.name] = t;
    return false;
}

} // namespace TOZ3_V2
