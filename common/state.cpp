#include "state.h"

#include <complex>
#include <cstdio>
#include <ostream>

#include "complex_type.h"
#include "lib/exceptions.h"
#include "scope.h"

namespace TOZ3_V2 {

P4Z3Instance *P4State::gen_instance(cstring name, const IR::Type *type,
                                    uint64_t id) {
    P4Z3Instance *instance;
    if (auto tn = type->to<IR::Type_Name>()) {
        type = resolve_type(tn);
    }
    if (auto ts = type->to<IR::Type_Struct>()) {
        instance = new StructInstance(this, ts, id);
    } else if (auto te = type->to<IR::Type_Header>()) {
        instance = new HeaderInstance(this, te, id);
    } else if (auto te = type->to<IR::Type_Enum>()) {
        instance = new EnumInstance(this, te, id);
    } else if (auto te = type->to<IR::Type_Error>()) {
        instance = new ErrorInstance(this, te, id);
    } else if (auto te = type->to<IR::Type_Extern>()) {
        instance = new ExternInstance(this, te);
    } else if (auto tbi = type->to<IR::Type_Bits>()) {
        instance = new Z3Wrapper(ctx->bv_const(name, tbi->width_bits()));
    } else if (auto tvb = type->to<IR::Type_Varbits>()) {
        instance = new Z3Wrapper(ctx->bv_const(name, tvb->width_bits()));
    } else if (type->is<IR::Type_Boolean>()) {
        instance = new Z3Wrapper(ctx->bool_const(name));
    } else {
        BUG("Type \"%s\" not supported!.", type);
    }
    add_to_allocated_vars(instance);
    return instance;
}

z3::expr P4State::gen_z3_expr(cstring name, const IR::Type *type) {
    if (auto tbi = type->to<IR::Type_Bits>()) {
        return ctx->bv_const(name, tbi->width_bits());
    } else if (auto tvb = type->to<IR::Type_Varbits>()) {
        return ctx->bv_const(name, tvb->width_bits());
    } else if (type->is<IR::Type_Boolean>()) {
        return ctx->bool_const(name);
    }
    BUG("Type \"%s\" not supported for Z3 expressions!.", type);
}

void P4State::push_scope() {
    P4Scope *scope = new P4Scope();
    scopes.push_back(scope);
    add_to_allocated_scopes(scope);
}

void P4State::pop_scope() { scopes.pop_back(); }
P4Scope *P4State::get_current_scope() { return scopes.back(); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    P4Scope *target_scope = nullptr;
    find_type(type_name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Variable %s already exists in target scope.", type_name);
    } else {
        if (scopes.empty()) {
            main_scope->add_type(type_name, t);
            // assume we insert into the global scope
        } else {
            scopes.back()->add_type(type_name, t);
        }
    }
}

const IR::Type *P4State::get_type(cstring type_name) {
    for (P4Scope *scope : scopes) {
        if (scope->has_type(type_name)) {
            return scope->get_type(type_name);
        }
    }
    // also check the parent scope
    return main_scope->get_type(type_name);
}

const IR::Type *P4State::resolve_type(const IR::Type *type) {
    const IR::Type *ret_type = type;
    if (auto tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        return get_type(type_name);
    }
    return ret_type;
}

const IR::Type *P4State::find_type(cstring type_name, P4Scope **owner_scope) {
    for (P4Scope *scope : scopes) {
        if (scope->has_type(type_name)) {
            *owner_scope = scope;
            return scope->get_type(type_name);
        }
    }
    // also check the parent scope
    if (main_scope->has_type(type_name)) {
        *owner_scope = main_scope;
        return main_scope->get_type(type_name);
    }
    return nullptr;
}

P4Z3Instance *P4State::get_var(cstring name) {
    for (P4Scope *scope : scopes) {
        if (scope->has_var(name)) {
            return scope->get_var(name);
        }
    }
    // also check the parent scope
    return main_scope->get_var(name);
}

P4Z3Instance *P4State::find_var(cstring name, P4Scope **owner_scope) {
    for (P4Scope *scope : scopes) {
        if (scope->has_var(name)) {
            *owner_scope = scope;
            return scope->get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope->has_var(name)) {
        *owner_scope = main_scope;
        return main_scope->get_var(name);
    }
    return nullptr;
}

void P4State::update_var(cstring name, P4Z3Instance *var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        target_scope->update_var(name, var);
    } else {
        FATAL_ERROR("Variable %s not found.", name);
    }
}

void P4State::declare_local_var(cstring name, P4Z3Instance *var) {
    if (scopes.empty()) {
        // assume we insert into the global scope
        main_scope->declare_var(name, var);
    } else {
        scopes.back()->declare_var(name, var);
    }
}

void P4State::declare_var(cstring name, const IR::Declaration *decl) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Variable %s already exists in target scope.", name);
    } else {
        auto decl_instance = new P4Declaration(decl);
        add_to_allocated_vars(decl_instance);
        if (scopes.empty()) {
            main_scope->declare_var(name, decl_instance);
            // assume we insert into the global scope
        } else {
            scopes.back()->declare_var(name, decl_instance);
        }
    }
}

ProgState *P4State::clone_state() {
    ProgState *cloned_state = new ProgState();

    for (P4Scope *scope : scopes) {
        P4Scope *cloned_scope = new P4Scope();
        add_to_allocated_scopes(cloned_scope);
        P4Z3Instance *member_cpy;
        for (auto value_tuple : *scope->get_var_map()) {
            auto var_name = value_tuple.first;
            auto var = value_tuple.second;
            if (auto z3_var = var->to<Z3Wrapper>()) {
                member_cpy = new Z3Wrapper(*z3_var);
            } else if (auto complex_var = var->to<StructInstance>()) {
                member_cpy = new StructInstance(*complex_var);
            } else if (auto complex_var = var->to<HeaderInstance>()) {
                member_cpy = new HeaderInstance(*complex_var);
            } else if (auto int_var = var->to<Z3Int>()) {
                member_cpy = new Z3Int(*int_var);
            } else {
                BUG("Var is neither type z3::expr nor StructInstance!");
            }
            add_to_allocated_vars(member_cpy);
            cloned_scope->declare_var(var_name, member_cpy);
        }
        cloned_state->push_back(cloned_scope);
    }
    cloned_states.push_back(cloned_state);
    return cloned_state;
}

ProgState P4State::fork_state() {
    ProgState old_prog_state = scopes;
    ProgState new_prog_state = ProgState();

    for (P4Scope *scope : old_prog_state) {
        P4Scope *cloned_scope = new P4Scope();
        add_to_allocated_scopes(cloned_scope);
        P4Z3Instance *member_cpy;
        for (auto value_tuple : *scope->get_var_map()) {
            auto var_name = value_tuple.first;
            auto var = value_tuple.second;
            if (auto z3_var = var->to<Z3Wrapper>()) {
                member_cpy = new Z3Wrapper(*z3_var);
            } else if (auto complex_var = var->to<StructInstance>()) {
                member_cpy = new StructInstance(*complex_var);
            } else if (auto complex_var = var->to<HeaderInstance>()) {
                member_cpy = new HeaderInstance(*complex_var);
            } else if (auto int_var = var->to<Z3Int>()) {
                member_cpy = new Z3Int(*int_var);
            } else {
                BUG("Var is neither type z3::expr nor StructInstance!");
            }
            add_to_allocated_vars(member_cpy);
            cloned_scope->declare_var(var_name, member_cpy);
        }
        new_prog_state.push_back(cloned_scope);
    }
    scopes = new_prog_state;
    return old_prog_state;
}

void merge_var_maps(z3::expr *cond, std::map<cstring, P4Z3Instance *> *then_map,
                    const std::map<cstring, P4Z3Instance *> *else_map) {
    for (auto then_tuple : *then_map) {
        cstring then_name = then_tuple.first;
        P4Z3Instance *then_var = then_tuple.second;
        const P4Z3Instance *else_var = else_map->at(then_name);
        then_var->merge(cond, else_var);
    }
}

void P4State::merge_state(z3::expr cond, const ProgState *else_state) {
    for (size_t i = 1; i < scopes.size(); ++i) {
        P4Scope *then_scope = scopes[i];
        const P4Scope *else_scope = else_state->at(i);
        merge_var_maps(&cond, then_scope->get_var_map(),
                       else_scope->get_const_var_map());
    }
}
} // namespace TOZ3_V2
