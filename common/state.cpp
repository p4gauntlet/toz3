#include <complex>
#include <cstdio>
#include <ostream>

#include "complex_type.h"
#include "scope.h"
#include "state.h"

namespace TOZ3_V2 {

P4Z3Instance P4State::gen_instance(cstring name, const IR::Type *type,
                                   uint64_t id) {
    if (auto ts = type->to<IR::Type_StructLike>()) {
        auto instance = new StructInstance(this, ts, id);
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

void P4State::add_scope(P4Scope *scope) { scopes.push_back(scope); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    type_map[type_name] = t;
}

const IR::Type *P4State::get_type(cstring type_name) {
    if (type_map.count(type_name)) {
        return type_map[type_name];
    } else {
        BUG("Type name \"%s\" not found!.", type_name);
    }
    return nullptr;
}

const IR::Type *P4State::resolve_type(const IR::Type *type) {
    const IR::Type *ret_type = type;
    if (auto tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        return get_type(type_name);
    }
    return ret_type;
}

void P4State::add_decl(cstring decl_name, const IR::Declaration *d) {
    decl_map[decl_name] = d;
}

const IR::Declaration *P4State::get_decl(cstring decl_name) {
    if (decl_map.count(decl_name)) {
        return decl_map[decl_name];
    } else {
        BUG("Decl name \"%s\" not found!.", decl_name);
    }
    return nullptr;
}

P4Z3Instance P4State::find_var(cstring name, P4Scope **owner_scope) {
    for (P4Scope *scope : scopes) {
        if (scope->value_map.count(name)) {
            *owner_scope = scope;
            return scope->value_map.at(name);
        }
    }
    return nullptr;
}

P4Z3Instance P4State::get_var(cstring name) {
    for (P4Scope *scope : scopes) {
        if (scope->value_map.count(name)) {
            return scope->value_map.at(name);
        }
    }
    return nullptr;
}

void P4State::insert_var(cstring name, P4Z3Instance var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        target_scope->value_map.insert({name, var});
    } else {
        scopes.back()->value_map.insert({name, var});
    }
}

std::vector<P4Scope *> P4State::checkpoint() {
    std::vector<P4Scope *> old_scopes = scopes;
    std::vector<P4Scope *> cloned_scopes;

    scopes = cloned_scopes;
    for (P4Scope *scope : old_scopes) {
        P4Scope *cloned_scope = new P4Scope();
        std::map<cstring, P4Z3Instance> cloned_map;
        for (auto value_tuple : scope->value_map) {
            auto var_name = value_tuple.first;
            auto var = value_tuple.second;
            if (auto complex_var = check_complex<StructInstance>(var)) {
                P4Z3Instance cloned_var = new StructInstance(*complex_var);
                cloned_scope->value_map.insert({var_name, cloned_var});
            } else {
                cloned_scope->value_map.insert({var_name, var});
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
        P4Z3Instance then_var = then_tuple.second;
        P4Z3Instance else_var = else_map->at(then_name);
        if (z3::expr *z3_then_var = boost::get<z3::expr>(&then_var)) {
            if (z3::expr *z3_else_var = boost::get<z3::expr>(&else_var)) {
                z3::expr merged_expr =
                    z3::ite(cond, *z3_then_var, *z3_else_var);
                then_map->at(then_name) = merged_expr;
            } else {
                BUG("Z3 Expr Merge not yet supported. ");
            }
        } else if (auto z3_member_var = check_complex<Z3Int>(then_var)) {
            if (auto *z3_else_var = check_complex<Z3Int>(else_var)) {
                z3::expr merged_expr =
                    z3::ite(cond, z3_member_var->val, z3_else_var->val);
                then_map->at(then_name) = merged_expr;
            } else {
                BUG("Int merge with other type not yet supported. ");
            }
        } else if (auto z3_then_var = check_complex<StructInstance>(then_var)) {
            if (auto z3_else_var = check_complex<StructInstance>(else_var)) {
                merge_var_maps(cond, &z3_then_var->members,
                               &z3_else_var->members);
            } else {
                BUG("Z3 Struct Merge not yet supported. ");
            }
        } else {
            BUG("Merge not supported. ");
        }
    }
}

void P4State::merge_state(z3::expr cond, std::vector<P4Scope *> then_state,
                          std::vector<P4Scope *> else_state) {
    for (size_t i = 1; i < then_state.size(); ++i) {
        P4Scope *then_scope = then_state[i];
        P4Scope *else_scope = else_state[i];
        merge_var_maps(cond, &then_scope->value_map, &else_scope->value_map);
    }
}
} // namespace TOZ3_V2
