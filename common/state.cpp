#include "state.h"
#include "complex_type.h"
#include <cstdio>

namespace TOZ3_V2 {

P4Scope::P4Scope(const P4Scope &other) {
    for (auto value_tuple : other.value_map) {
        cstring name = value_tuple.first;
        P4Z3Instance var = value_tuple.second;
        if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
            value_map.insert({name, *z3_var});
        } else if (StructInstance *z3_var =
                       check_complex<StructInstance>(var)) {
            StructInstance member_cpy = *z3_var;
            value_map.insert({name, &member_cpy});
        } else {
        }
    }
}

// overload = operator
P4Scope &P4Scope::operator=(const P4Scope &other) {
    if (this == &other)
        return *this; // self assignment

    for (auto value_tuple : other.value_map) {
        cstring name = value_tuple.first;
        P4Z3Instance var = value_tuple.second;
        if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
            value_map.insert({name, *z3_var});
        } else if (auto z3_var = check_complex<StructInstance>(var)) {
            StructInstance member_cpy = *z3_var;
            value_map.insert({name, &member_cpy});
        } else {
        }
    }
    return *this;
}

P4Z3Instance P4State::gen_instance(cstring name, const IR::Type *type,
                                   uint64_t id) {
    if (auto ts = type->to<IR::Type_StructLike>()) {
        return new StructInstance(this, ts, id);
    } else if (auto te = type->to<IR::Type_Enum>()) {
        return new EnumInstance(this, te, id);
    } else if (auto te = type->to<IR::Type_Error>()) {
        return new ErrorInstance(this, te, id);
    } else if (auto te = type->to<IR::Type_Extern>()) {
        return new ExternInstance(this, te);
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
    std::vector<P4Scope *> cloned_scopes;
    for (auto scope : scopes) {
        P4Scope cloned_scope = *scope;
        cloned_scopes.push_back(&cloned_scope);
    }
    return scopes;
}

void P4State::merge_state(z3::expr cond, std::vector<P4Scope *> then_state,
                          std::vector<P4Scope *> else_state) {
    for (size_t i = 1; i < then_state.size(); ++i) {
        auto then_scope = then_state[i];
        auto else_scope = else_state[i];
        for (auto then_tuple : then_scope->value_map) {
            cstring then_name = then_tuple.first;
            P4Z3Instance then_var = then_tuple.second;
            P4Z3Instance else_var = else_scope->value_map.at(then_name);
            if (z3::expr *z3_then_var = boost::get<z3::expr>(&then_var)) {
                if (z3::expr *z3_else_var = boost::get<z3::expr>(&else_var)) {
                    z3::expr merged_expr =
                        z3::ite(cond, *z3_then_var, *z3_else_var);
                    then_scope->value_map.insert({then_name, merged_expr});
                } else {
                    BUG("Z3 Expr Merge not yet supported. ");
                }
            } else if (auto z3_var = check_complex<Z3Int>(then_var)) {
                BUG("Int Merge not supported. ");
            } else if (auto z3_then_var =
                           check_complex<StructInstance>(then_var)) {
                if (auto z3_else_var =
                        check_complex<StructInstance>(else_var)) {
                    z3_then_var->merge(cond, z3_else_var);
                } else {
                    BUG("Z3 Struct Merge not yet supported. ");
                }
            } else {
                BUG("Merge not supported. ");
            }
        }
    }
}
} // namespace TOZ3_V2
