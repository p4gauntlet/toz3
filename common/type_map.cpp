#include <cstdio>
#include <utility>

#include "complex_type.h"
#include "ir/ir-generated.h"
#include "lib/exceptions.h"
#include "type_map.h"

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
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Enum *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Error *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Extern *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Package *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Parser *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Control *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::P4Parser *p) {
    state->add_type(p->name.name, p);
    return false;
}

bool TypeVisitor::preorder(const IR::P4Control *c) {
    state->add_type(c->name.name, c);
    return false;
}

bool TypeVisitor::preorder(const IR::Method *m) {
    state->declare_var(m->name.name, m);
    return false;
}

bool TypeVisitor::preorder(const IR::P4Action *a) {
    state->declare_var(a->name.name, a);
    return false;
}

bool TypeVisitor::preorder(const IR::P4Table *t) {
    state->declare_var(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Instance *di) {
    state->declare_var(di->name.name, di);
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_MatchKind *) {
    // TODO: Figure out purpose of Declaration_MatchKind
    // state->add_decl(dm->name.name, dm);
    return false;
}

} // namespace TOZ3_V2
