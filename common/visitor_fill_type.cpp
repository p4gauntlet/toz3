#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

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
    // FIXME: Enums are really nasty because we also need to access them
    state->add_type(t->name.name, t);
    state->declare_var(t->name.name, new EnumInstance(state, t, 0), t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Error *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Extern *t) {
    state->add_type(t->name.name, t);
    // state->declare_var(t->name.name, new ExternInstance(state, t), t);

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
    auto num_params = 0;
    auto num_optional_params = 0;
    for (auto param : m->getParameters()->parameters) {
        if (param->isOptional()) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto decl = new P4Declaration(m);
    for (auto idx = 0; idx <= num_optional_params; ++idx) {
        // The IR has bizarre side effects when storing pointers in a map
        // FIXME: Think about how to simplify this, maybe use their vector
        auto name = overloaded_name + std::to_string(num_params + idx);
        state->declare_static_decl(name, decl);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::P4Action *a) {
    // FIXME: Overloading uses num of parameters, it should use types
    cstring overloaded_name = a->getName().name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (auto param : a->getParameters()->parameters) {
        if (param->direction == IR::Direction::None) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto decl = new P4Declaration(a);
    cstring name_basic = overloaded_name + std::to_string(num_params);
    state->declare_static_decl(name_basic, decl);
    // The IR has bizarre side effects when storing pointers in a map
    // FIXME: Think about how to simplify this, maybe use their vector
    if (num_optional_params != 0) {
        cstring name_opt =
            overloaded_name + std::to_string(num_params + num_optional_params);
        state->declare_static_decl(name_opt, decl);
    }
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
        // Do not execute main here just yet.
        state->declare_static_decl(instance_name, new P4Declaration(di));
    } else {
        auto instance = state->gen_instance(instance_name, resolved_type);
        state->declare_var(instance_name, instance, resolved_type);
    }
    return false;
}
// new DeclarationInstance(state, instance_decl)
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
