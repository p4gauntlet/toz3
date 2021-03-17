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
    // FIXME: Overloading uses num of parameters, it should use types
    cstring overloaded_name = f->getName().name;
    auto num_params = f->getParameters()->parameters.size();
    // for (auto param : f->getParameters()->parameters) {
    //     overloaded_name += param->node_type_name();
    // }
    overloaded_name += std::to_string(num_params);
    state->declare_static_decl(overloaded_name, new P4Declaration(f));
    return false;
}

bool TypeVisitor::preorder(const IR::Method *m) {
    // FIXME: Overloading uses num of parameters, it should use types
    cstring overloaded_name = m->getName().name;
    auto num_params = m->getParameters()->parameters.size();
    // for (auto param : f->getParameters()->parameters) {
    //     overloaded_name += param->node_type_name();
    // }
    overloaded_name += std::to_string(num_params);
    state->declare_static_decl(overloaded_name, new P4Declaration(m));
    return false;
}

bool TypeVisitor::preorder(const IR::P4Action *a) {
    // FIXME: Overloading uses num of parameters, it should use types
    cstring overloaded_name = a->getName().name;
    auto num_params = a->getParameters()->parameters.size();
    // for (auto param : f->getParameters()->parameters) {
    //     overloaded_name += param->node_type_name();
    // }
    overloaded_name += std::to_string(num_params);
    state->declare_static_decl(overloaded_name, new P4Declaration(a));
    return false;
}

bool TypeVisitor::preorder(const IR::P4Table *t) {
    state->declare_static_decl(t->name.name, new P4TableInstance(state, t));
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Instance *di) {
    auto instance_name = di->getName().name;
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (auto spec_type = resolved_type->to<IR::Type_Specialized>()) {
        // FIXME: Figure out what do here
        // for (auto arg : *spec_type->arguments) {
        //     const IR::Type *resolved_arg = state->resolve_type(arg);
        // }
        resolved_type = state->resolve_type(spec_type->baseType);
    }

    if (instance_name == "main") {
        state->declare_static_decl(instance_name, new P4Declaration(di));
    } else {
        if (auto instance_decl = resolved_type->to<IR::Type_Declaration>()) {
            state->declare_var(instance_name,
                               new DeclarationInstance(state, instance_decl),
                               resolved_type);
        } else {
            P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ",
                              resolved_type, resolved_type->node_type_name());
        }
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left;
    if (dc->initializer) {
        dc->initializer->apply(resolve_expr);
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
        dv->initializer->apply(resolve_expr);
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
