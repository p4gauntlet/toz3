#include "state.h"

#include <complex>
#include <cstdio>
#include <ostream>

#include "lib/exceptions.h"

namespace TOZ3_V2 {

z3::expr z3_bv_cast(const z3::expr *expr, z3::sort dest_type) {
    uint64_t expr_size;
    uint64_t dest_size = dest_type.bv_size();
    if (expr->is_bv()) {
        expr_size = expr->get_sort().bv_size();
    } else if (expr->is_int()) {
        auto cast_val = z3::int2bv(dest_type.bv_size(), *expr).simplify();
        return z3::int2bv(dest_type.bv_size(), *expr).simplify();
    } else {
        BUG("Casting %s to a bit vector is not supported.",
            expr->to_string().c_str());
    }
    // At this point we are only dealing with expr bit vectors

    if (expr_size < dest_size) {
        // The target value is larger, extend with zeros
        return z3::zext(*expr, dest_size - expr_size);
    } else if (expr_size > dest_size) {
        // The target value is smaller, truncate everything on the right
        return expr->extract(dest_size - 1, 0);
    } else {
        // Nothing to do just return
        return *expr;
    }
}

z3::expr cast(P4State *, P4Z3Instance *expr, z3::sort dest_type) {
    if (dest_type.is_bv()) {
        if (auto z3_var = expr->to<Z3Bitvector>()) {
            return z3_bv_cast(&z3_var->val, dest_type);
        } else if (auto z3_var = expr->to<Z3Int>()) {
            return z3_bv_cast(&z3_var->val, dest_type);
        } else {
            BUG("Cast to bit vector type not supported.");
        }
    } else {
        BUG("Cast not supported.");
    }
}

P4Z3Instance *cast(P4State *state, P4Z3Instance *expr,
                   const IR::Type *dest_type) {
    if (auto tb = dest_type->to<IR::Type_Bits>()) {
        auto dest_sort = state->get_z3_ctx()->bv_sort(tb->width_bits());
        auto cast_val = cast(state, expr, dest_sort);
        Z3Bitvector wrapper = Z3Bitvector(cast_val);
        state->set_expr_result(wrapper);
        return state->copy_expr_result();
    } else if (dest_type->is<IR::Type_InfInt>()) {
        // FIXME: Clean this up
        if (auto z3_var = expr->to<Z3Bitvector>()) {
            cstring dec_str = z3_var->val.get_decimal_string(0);
            auto int_expr = state->get_z3_ctx()->int_val(dec_str.c_str());
            Z3Int wrapper = Z3Int(int_expr);
            state->set_expr_result(wrapper);
            return state->copy_expr_result();
        } else if (auto z3_var = expr->to<Z3Int>()) {
            cstring dec_str = z3_var->val.get_decimal_string(0);
            auto int_expr = state->get_z3_ctx()->int_val(dec_str.c_str());
            Z3Int wrapper = Z3Int(int_expr);
            state->set_expr_result(wrapper);
            return state->copy_expr_result();
        } else {
            BUG("Cast to bit vector type not supported.");
        }
    } else {
        BUG("Cast to type %s not supported", dest_type->node_type_name());
    }
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
        instance = new ExternInstance(this, te);
    } else if (type->is<IR::Type_Base>()) {
        instance = new Z3Bitvector(gen_z3_expr(name, type));
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
    P4Scope scope = P4Scope();
    scopes.push_back(scope);
}

void P4State::pop_scope() { scopes.pop_back(); }
P4Scope *P4State::get_current_scope() { return &scopes.back(); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    P4Scope *target_scope = nullptr;
    find_type(type_name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Variable %s already exists in target scope.", type_name);
    } else {
        if (scopes.empty()) {
            main_scope.add_type(type_name, t);
            // assume we insert into the global scope
        } else {
            get_current_scope()->add_type(type_name, t);
        }
    }
}

const IR::Type *P4State::get_type(cstring type_name) {
    for (P4Scope scope : scopes) {
        if (scope.has_type(type_name)) {
            return scope.get_type(type_name);
        }
    }
    // also check the parent scope
    return main_scope.get_type(type_name);
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
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        auto scope = &scopes.at(i);
        if (scope->has_type(type_name)) {
            *owner_scope = scope;
            return scope->get_type(type_name);
        }
    }
    for (P4Scope scope : scopes) {
    }
    // also check the parent scope
    if (main_scope.has_type(type_name)) {
        *owner_scope = &main_scope;
        return main_scope.get_type(type_name);
    }
    return nullptr;
}

P4Z3Instance *P4State::get_var(cstring name) {
    for (P4Scope scope : scopes) {
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

void P4State::declare_local_var(cstring name, P4Z3Instance *var) {
    if (scopes.empty()) {
        // assume we insert into the global scope
        main_scope.declare_var(name, var);
    } else {
        get_current_scope()->declare_var(name, var);
    }
}

const P4Declaration *P4State::get_static_decl(cstring name) {
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

const P4Declaration *P4State::find_static_decl(cstring name,
                                               P4Scope **owner_scope) {
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

void P4State::declare_static_decl(cstring name, const IR::Declaration *decl) {
    P4Scope *target_scope = nullptr;
    find_static_decl(name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Variable %s already exists in target scope.", name);
    } else {
        if (scopes.empty()) {
            main_scope.declare_static_decl(name, decl);
            // assume we insert into the global scope
        } else {
            get_current_scope()->declare_static_decl(name, decl);
        }
    }
}

ProgState P4State::clone_state() {
    ProgState cloned_state = ProgState();

    for (P4Scope scope : scopes) {
        P4Scope cloned_scope = P4Scope();
        P4Z3Instance *member_cpy;
        for (auto value_tuple : *scope.get_var_map()) {
            auto var_name = value_tuple.first;
            member_cpy = value_tuple.second->copy();
            add_to_allocated_vars(member_cpy);
            cloned_scope.declare_var(var_name, member_cpy);
        }
        cloned_state.push_back(cloned_scope);
    }
    return cloned_state;
}

ProgState P4State::fork_state() {
    ProgState old_prog_state = scopes;
    ProgState new_prog_state = ProgState();

    for (P4Scope scope : old_prog_state) {
        P4Scope cloned_scope = P4Scope();
        P4Z3Instance *member_cpy;
        for (auto value_tuple : *scope.get_var_map()) {
            auto var_name = value_tuple.first;
            member_cpy = value_tuple.second->copy();
            add_to_allocated_vars(member_cpy);
            cloned_scope.declare_var(var_name, member_cpy);
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
        P4Scope *then_scope = &scopes[i];
        const P4Scope *else_scope = &else_state->at(i);
        merge_var_maps(&cond, then_scope->get_var_map(),
                       else_scope->get_const_var_map());
    }
}
} // namespace TOZ3_V2
