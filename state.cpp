#include "state.h"
#include "complex_type.h"

namespace TOZ3_V2 {

boost::any P4State::gen_instance(cstring name, const IR::Type *type) {
    if (auto ts = type->to<IR::Type_StructLike>()) {
        return StructInstance(this, ts, 0);
    } else if (auto te = type->to<IR::Type_Enum>()) {
        return EnumInstance(this, te);
    } else if (auto te = type->to<IR::Type_Error>()) {
        return ErrorInstance(this, te);
    } else if (auto te = type->to<IR::Type_Extern>()) {
        return ExternInstance(this, te);
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

const IR::Type *P4State::resolve_type(const IR::Type *type) {
    const IR::Type *ret_type = type;
    if (auto tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        if (type_map.count(type_name)) {
            ret_type = type_map[type_name];
        } else {
            BUG("Type name \"%s\" not found!.", type_name);
        }
    }
    return ret_type;
}

boost::any *P4State::find_var(cstring name, P4Scope **owner_scope) {
    for (P4Scope *scope : scopes) {
        if (scope->value_map.count(name)) {
            *owner_scope = scope;
            return &scope->value_map.at(name);
        }
    }
    return nullptr;
}

void P4State::insert_var(cstring name, boost::any var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        target_scope->value_map[name] = var;
    } else {
        scopes.back()->value_map[name] = var;
    }
}

} // namespace TOZ3_V2
