#include "state.h"

#include <complex>
#include <cstdio>
#include <ostream>

#include "lib/exceptions.h"

namespace TOZ3_V2 {

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
        instance = new ExternInstance(te);
    } else if (type->is<IR::Type_Void>()) {
        instance = new VoidResult();
    } else if (type->is<IR::Type_Base>()) {
        instance = new Z3Bitvector(this, gen_z3_expr(name, type));
    } else {
        BUG("Type \"%s\" not supported!.", type);
    }
    return instance;
}

void P4State::push_scope() { scopes.push_back(P4Scope()); }

void P4State::pop_scope() { scopes.pop_back(); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    P4Scope *target_scope = nullptr;
    find_type(type_name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Type %s already exists in target scope.", type_name);
    } else {
        if (scopes.empty()) {
            main_scope.add_type(type_name, t);
            // assume we insert into the global scope
        } else {
            get_mut_current_scope()->add_type(type_name, t);
        }
    }
}

const IR::Type *P4State::get_type(cstring type_name) const {
    for (auto &scope : scopes) {
        if (scope.has_type(type_name)) {
            return scope.get_type(type_name);
        }
    }
    // also check the parent scope
    return main_scope.get_type(type_name);
}

const IR::Type *P4State::resolve_type(const IR::Type *type) const {
    if (auto tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        return get_type(type_name);
    }
    return type;
}

const IR::Type *P4State::find_type(cstring type_name, P4Scope **owner_scope) {
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        auto scope = &scopes.at(i);
        if (scope->has_type(type_name)) {
            *owner_scope = scope;
            return scope->get_type(type_name);
        }
    }
    // also check the parent scope
    if (main_scope.has_type(type_name)) {
        *owner_scope = &main_scope;
        return main_scope.get_type(type_name);
    }
    return nullptr;
}

P4Z3Instance *P4State::get_var(cstring name) const {
    for (auto &scope : scopes) {
        if (scope.has_var(name)) {
            return scope.get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        return main_scope.get_var(name);
    }
    error("Variable %s not found in scope.", name);
    exit(1);
}

const IR::Type *P4State::get_var_type(cstring name) const {
    for (auto &scope : scopes) {
        if (scope.has_var(name)) {
            return scope.get_var_type(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        return main_scope.get_var_type(name);
    }
    error("Variable %s not found in scope.", name);
    exit(1);
}

P4Z3Instance *P4State::find_var(cstring name, P4Scope **owner_scope) {
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        auto scope = &scopes.at(i);
        if (scope->has_var(name)) {
            *owner_scope = scope;
            return scope->get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        *owner_scope = &main_scope;
        return main_scope.get_var(name);
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

void P4State::declare_var(cstring name, P4Z3Instance *var,
                          const IR::Type *decl_type) {
    if (scopes.empty()) {
        // assume we insert into the global scope
        main_scope.declare_var(name, var, decl_type);
    } else {
        get_mut_current_scope()->declare_var(name, var, decl_type);
    }
}

const P4Declaration *P4State::get_static_decl(cstring name) const {
    for (auto scope = scopes.begin(); scope != scopes.end(); ++scope) {
        if (scope->has_static_decl(name)) {
            return scope->get_static_decl(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_static_decl(name)) {
        return main_scope.get_static_decl(name);
    }
    error("Static Declaration %s not found in scope.", name);
    exit(1);
}

P4Declaration *P4State::find_static_decl(cstring name, P4Scope **owner_scope) {
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        auto scope = &scopes.at(i);
        if (scope->has_static_decl(name)) {
            *owner_scope = scope;
            return scope->get_static_decl(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_static_decl(name)) {
        *owner_scope = &main_scope;
        return main_scope.get_static_decl(name);
    }
    return nullptr;
}

void P4State::declare_static_decl(cstring name, P4Declaration *decl) {
    P4Scope *target_scope = nullptr;
    find_static_decl(name, &target_scope);
    if (target_scope) {
        warning("Declaration %s shadows existing declaration.", decl->decl);
    }
    if (scopes.empty()) {
        main_scope.declare_static_decl(name, decl);
        // assume we insert into the global scope
    } else {
        get_mut_current_scope()->declare_static_decl(name, decl);
    }
}

ProgState P4State::clone_state() const {
    auto cloned_state = ProgState();

    for (auto &scope : scopes) {
        cloned_state.push_back(scope.clone());
    }
    return cloned_state;
}

VarMap P4State::clone_vars() const {
    VarMap cloned_vars;
    // this also implicitly shadows
    for (auto &scope : scopes) {
        auto sub_vars = scope.clone_vars();
        cloned_vars.insert(sub_vars.begin(), sub_vars.end());
    }
    return cloned_vars;
}

VarMap P4State::get_vars() const {
    VarMap concat_map;
    // this also implicitly shadows
    for (auto &scope : scopes) {
        auto sub_vars = scope.get_var_map();
        concat_map.insert(sub_vars.begin(), sub_vars.end());
    }
    return concat_map;
}

void P4State::restore_vars(const VarMap &input_map) {
    // this also implicitly shadows
    for (auto &map_tuple : input_map) {
        update_var(map_tuple.first, map_tuple.second.first);
    }
}

void P4State::merge_vars(const z3::expr &cond, const VarMap &then_map) {
    // this also implicitly shadows
    for (auto &map_tuple : get_vars()) {
        auto else_name = map_tuple.first;
        auto instance = map_tuple.second.first;
        // TODO: This check should not be necessary
        // Find a cleaner way using scopes
        auto then_instance = then_map.find(else_name);
        if (then_instance != then_map.end()) {
            instance->merge(cond, *then_instance->second.first);
        }
    }
}

void merge_var_maps(const z3::expr &cond, const VarMap &then_map,
                    const VarMap &else_map) {
    for (auto &then_tuple : then_map) {
        auto then_name = then_tuple.first;
        auto then_var = then_tuple.second.first;
        // TODO: This check should not be necessary
        // Find a cleaner way using scopes
        auto else_var = else_map.find(then_name);
        if (else_var != else_map.end()) {
            then_var->merge(cond, *else_var->second.first);
        }
    }
}

void P4State::merge_state(const z3::expr &cond, const ProgState &else_state) {
    for (size_t i = 0; i < scopes.size(); ++i) {
        auto then_scope = &scopes[i];
        auto else_scope = &else_state.at(i);
        merge_var_maps(cond, then_scope->get_var_map(),
                       else_scope->get_var_map());
    }
}
} // namespace TOZ3_V2
