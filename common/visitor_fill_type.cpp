#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

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

bool TypeVisitor::preorder(const IR::Type_Typedef *t) {
    state->add_type(t->name.name, t->type);
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

bool TypeVisitor::preorder(const IR::Function *f) {
    // FIXME: Overloading
    state->declare_static_decl(f->name.name, new P4Declaration(f));
    return false;
}

bool TypeVisitor::preorder(const IR::Method *m) {
    // FIXME: Overloading
    cstring overloaded_name = m->name.name;
    for (auto param : m->getParameters()->parameters) {
        overloaded_name += param->node_type_name();
    }
    state->declare_static_decl(overloaded_name, new P4Declaration(m));
    return false;
}

bool TypeVisitor::preorder(const IR::P4Action *a) {
    state->declare_static_decl(a->name.name, new P4Declaration(a));
    return false;
}

bool TypeVisitor::preorder(const IR::P4Table *t) {
    state->declare_static_decl(t->name.name, new P4TableInstance(state, t));
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Instance *di) {
    state->declare_static_decl(di->name.name, new P4Declaration(di));
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left;
    if (dc->initializer) {
        dc->initializer->apply_visitor_preorder(resolve_expr);
        left = state->get_expr_result()->cast_allocate(dc->type);
    } else {
        left = state->gen_instance("undefined", dc->type);
    }
    state->declare_var(dc->name.name, left, dc->type);
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left;
    if (dv->initializer) {
        dv->initializer->apply_visitor_preorder(resolve_expr);
        left = state->get_expr_result()->cast_allocate(dv->type);
    } else {
        left = state->gen_instance("undefined", dv->type);
    }
    state->declare_var(dv->name.name, left, dv->type);

    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_MatchKind *) {
    // TODO: Figure out purpose of Declaration_MatchKind
    // state->add_decl(dm->name.name, dm);
    return false;
}

} // namespace TOZ3_V2
