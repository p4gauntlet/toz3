#include "state.h"

#include <complex>
#include <cstdio>
#include <ostream>

#include "complex_type.h"
#include "lib/exceptions.h"
#include "scope.h"

namespace TOZ3_V2 {

P4Z3Instance P4State::gen_instance(cstring name, const IR::Type *type,
                                   uint64_t id) {
    if (auto tn = type->to<IR::Type_Name>()) {
        type = resolve_type(tn);
    }
    if (auto ts = type->to<IR::Type_Struct>()) {
        auto instance = new StructInstance(this, ts, id);
        add_to_allocated(instance);
        return instance;
    } else if (auto te = type->to<IR::Type_Header>()) {
        auto instance = new HeaderInstance(this, te, id);
        add_to_allocated(instance);
        return instance;
    } else if (auto te = type->to<IR::Type_Enum>()) {
        auto instance = new EnumInstance(this, te, id);
        add_to_allocated(instance);
        return instance;
    } else if (auto te = type->to<IR::Type_Error>()) {
        auto instance = new ErrorInstance(this, te, id);
        add_to_allocated(instance);
        return instance;
    } else if (auto te = type->to<IR::Type_Extern>()) {
        auto instance = new ExternInstance(this, te);
        add_to_allocated(instance);
        return instance;
    } else if (auto tbi = type->to<IR::Type_Bits>()) {
        return ctx->bv_const(name, tbi->width_bits());
    } else if (auto tvb = type->to<IR::Type_Varbits>()) {
        return ctx->bv_const(name, tvb->width_bits());
    } else if (type->is<IR::Type_Boolean>()) {
        return ctx->bool_const(name);
    }
    BUG("Type \"%s\" not supported!.", type);
} // namespace TOZ3_V2

void P4State::push_scope() {
    P4Scope *scope = new P4Scope();
    scopes.push_back(scope);
}

void P4State::pop_scope() { scopes.pop_back(); }

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

void P4State::update_var(cstring name, P4Z3Instance var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        target_scope->update_var(name, var);
    } else {
        FATAL_ERROR("Variable %s not found.", name);
    }
}

void P4State::declare_local_var(cstring name, P4Z3Instance var) {
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
        add_to_allocated(decl_instance);
        if (scopes.empty()) {
            main_scope->declare_var(name, decl_instance);
            // assume we insert into the global scope
        } else {
            scopes.back()->declare_var(name, decl_instance);
        }
    }
}

std::vector<P4Scope *> P4State::checkpoint() {
    std::vector<P4Scope *> old_scopes = scopes;
    std::vector<P4Scope *> cloned_scopes;

    scopes = cloned_scopes;
    for (P4Scope *scope : old_scopes) {
        P4Scope *cloned_scope = new P4Scope();
        std::map<cstring, P4Z3Instance> cloned_map;
        for (auto value_tuple : *scope->get_var_map()) {
            auto var_name = value_tuple.first;
            auto var = &value_tuple.second;
            if (auto complex_var = to_type<StructBase>(var)) {
                P4Z3Instance cloned_var = new StructBase(*complex_var);
                cloned_scope->declare_var(var_name, cloned_var);
            } else {
                cloned_scope->declare_var(var_name, *var);
            }
        }
        scopes.push_back(cloned_scope);
    }
    return old_scopes;
}

void P4State::merge_var_maps(z3::expr cond,
                             std::map<cstring, P4Z3Instance> *then_map,
                             std::map<cstring, P4Z3Instance> *else_map) {
    for (auto then_tuple : *then_map) {
        cstring then_name = then_tuple.first;
        P4Z3Instance *then_var = &then_tuple.second;
        P4Z3Instance *else_var = &else_map->at(then_name);
        if (z3::expr *z3_then_var = to_type<z3::expr>(then_var)) {
            if (z3::expr *z3_else_var = to_type<z3::expr>(else_var)) {
                z3::expr merged_expr =
                    z3::ite(cond, *z3_then_var, *z3_else_var);
                then_map->at(then_name) = merged_expr;
            } else {
                BUG("Z3 Expr Merge not yet supported. ");
            }
        } else if (auto z3_member_var = to_type<Z3Int>(then_var)) {
            if (auto *z3_else_var = to_type<Z3Int>(else_var)) {
                z3::expr merged_expr =
                    z3::ite(cond, z3_member_var->val, z3_else_var->val);
                then_map->at(then_name) = merged_expr;
            } else {
                BUG("Int merge with other type not yet supported. ");
            }
        } else if (auto z3_then_var = to_type<StructBase>(then_var)) {
            if (auto z3_else_var = to_type<StructBase>(else_var)) {
                merge_var_maps(cond, z3_then_var->get_member_map(),
                               z3_else_var->get_member_map());
            } else {
                BUG("Z3 Struct Merge not yet supported. ");
            }
        } else if (auto z3_then_var = to_type<P4Declaration>(then_var)) {
            // these are constant, do nothing
        } else {
            BUG("State merge not supported. ");
        }
    }
} // namespace TOZ3_V2

void P4State::merge_state(z3::expr cond, std::vector<P4Scope *> then_state,
                          std::vector<P4Scope *> else_state) {
    for (size_t i = 1; i < then_state.size(); ++i) {
        P4Scope *then_scope = then_state[i];
        P4Scope *else_scope = else_state[i];
        merge_var_maps(cond, then_scope->get_var_map(),
                       else_scope->get_var_map());
    }
}
} // namespace TOZ3_V2
