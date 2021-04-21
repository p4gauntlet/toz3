#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "type_base.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool TypeVisitor::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (const auto *o : p->objects) {
        visit(o);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Type_StructLike *t) {
    state->add_type(t->name.name, t);
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Enum *t) {
    // TODO: Enums are really nasty because we also need to access them
    // TODO: Simplify this.
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s",
                  enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, new EnumInstance(state, t, 0, ""), t);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Error *t) {
    // TODO: Simplify this.
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s",
                  enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, new ErrorInstance(state, t, 0, ""), t);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Type_SerEnum *t) {
    // TODO: Enums are really nasty because we also need to access them
    // TODO: Simplify this.
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s",
                  enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        ordered_map<cstring, P4Z3Instance *> input_members;
        const auto *member_type = t->type;
        for (const auto *member : t->members) {
            member->value->apply(resolve_expr);
            input_members.emplace(
                member->name.name,
                state->get_expr_result()->cast_allocate(member_type));
        }
        state->add_type(name, t);
        state->declare_var(
            name, new SerEnumInstance(state, input_members, t, 0, ""), t);
    }
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Extern *t) {
    state->add_type(t->name.name, t);

    return false;
}

bool TypeVisitor::preorder(const IR::Type_Typedef *t) {
    state->add_type(t->name.name, state->resolve_type(t->type));
    return false;
}

bool TypeVisitor::preorder(const IR::Type_Newtype *t) {
    state->add_type(t->name.name, state->resolve_type(t->type));
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
    cstring overloaded_name = f->name.name;
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
    cstring overloaded_name = m->name.name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : m->getParameters()->parameters) {
        if (param->isOptional()) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto *decl = new P4Declaration(m);
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
    cstring overloaded_name = a->name.name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : a->getParameters()->parameters) {
        if (param->direction == IR::Direction::None) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto *decl = new P4Declaration(a);
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
    auto instance_name = di->name.name;
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (const auto *spec_type = resolved_type->to<IR::Type_Specialized>()) {
        // FIXME: Figure out what do here
        // for (auto arg : *spec_type->arguments) {
        //     const IR::Type *resolved_arg = state->resolve_type(arg);
        // }
        resolved_type = state->resolve_type(spec_type->baseType);
    }
    // TODO: Figure out a way to process packages
    if (instance_name == "main" || resolved_type->is<IR::Type_Package>()) {
        // Do not execute main here just yet.
        state->declare_static_decl(instance_name, new P4Declaration(di));
    } else if (const auto *te = resolved_type->to<IR::Type_Extern>()) {
        // TODO: Clean this mess up.
        state->declare_var(instance_name, new ExternInstance(state, te), te);
    } else if (const auto *instance_decl =
                   resolved_type->to<IR::Type_Declaration>()) {
        std::vector<P4Z3Instance *> resolved_const_args;
        for (const auto *arg : *di->arguments) {
            arg->expression->apply(resolve_expr);
            resolved_const_args.push_back(state->copy_expr_result());
        }
        state->declare_var(
            di->name.name,
            new ControlInstance(state, instance_decl, resolved_const_args),
            resolved_type);
    } else {
        P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ",
                          resolved_type, resolved_type->node_type_name());
    }
    return false;
}

// new DeclarationInstance(state, instance_decl)
bool TypeVisitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left = nullptr;
    if (dc->initializer != nullptr) {
        dc->initializer->apply(resolve_expr);
        left = state->get_expr_result()->cast_allocate(dc->type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, dc->type);
    }
    state->declare_var(dc->name.name, left, dc->type);
    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left = nullptr;
    if (dv->initializer != nullptr) {
        dv->initializer->apply(resolve_expr);
        left = state->get_expr_result()->cast_allocate(dv->type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, dv->type);
    }
    state->declare_var(dv->name.name, left, dv->type);

    return false;
}

bool TypeVisitor::preorder(const IR::Declaration_MatchKind * /*dm */) {
    // TODO: Figure out purpose of Declaration_MatchKind
    // state->add_decl(dm->name.name, dm);
    return false;
}

}  // namespace TOZ3_V2
