#include "complex_type.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

P4Z3Instance cast(P4State *state, P4Z3Instance *expr,
                  const IR::Type *dest_type) {
    if (auto tb = dest_type->to<IR::Type_Bits>()) {
        if (z3::expr *z3_var = to_type<z3::expr>(expr)) {
            if (z3_var->get_sort().is_bv()) {
                return state->ctx->bv_val(z3_var->get_decimal_string(0).c_str(),
                                          dest_type->width_bits());
            } else {
                BUG("Cast type not supported ");
            }
        } else if (auto z3_var = to_type<Z3Int>(expr)) {
            auto val_string = z3_var->val.get_decimal_string(0);
            return state->ctx->bv_val(val_string.c_str(),
                                      dest_type->width_bits());
        } else {
            BUG("Cast from expr xpr to node %s supported ",
                dest_type->node_type_name());
        }
    } else {
        BUG("Cast to type %s not supported", dest_type->node_type_name());
    }
}

z3::expr z3_bv_cast(z3::expr *expr, z3::sort *dest_type) {
    uint64_t expr_size;
    uint64_t dest_size = dest_type->bv_size();
    if (expr->is_bv()) {
        expr_size = expr->get_sort().bv_size();
    } else if (expr->is_int()) {
        return z3::int2bv(dest_type->bv_size(), *expr);
    } else {
        BUG("Cast to z3 bit vector type not supported.");
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

z3::expr z3_cast(P4State *state, P4Z3Instance *expr, z3::sort *dest_type) {
    if (dest_type->is_bv()) {
        if (z3::expr *z3_var = to_type<z3::expr>(expr)) {
            return z3_bv_cast(z3_var, dest_type);
        } else if (auto z3_var = to_type<Z3Int>(expr)) {
            return z3_bv_cast(&z3_var->val, dest_type);
        } else {
            BUG("Cast to bit vector type not supported.");
        }
    } else {
        BUG("Cast not supported.");
    }
}

z3::expr merge_z3_expr(z3::expr *cond, z3::expr *then_expr,
                       const P4Z3Instance *else_expr) {
    if (const z3::expr *z3_var = to_const_type<z3::expr>(else_expr)) {
        return z3::ite(*cond, *then_expr, *z3_var);
    } else if (auto z3_var = to_const_type<Z3Int>(else_expr)) {
        return z3::ite(*cond, *then_expr, z3_var->val);
    } else {
        BUG("Z3 expression merge not supported.");
    }
}

void Z3Int::merge(z3::expr *cond, const P4ComplexInstance *other) {

    if (auto other_int = other->to<Z3Int>()) {
        val = z3::ite(*cond, val, other_int->val);
    }
    BUG("Unsupported merge class.");
}

void Z3Int::merge(z3::expr *cond, const z3::expr *other) {
    val = z3::ite(*cond, val, *other);
}

StructBase::StructBase(P4State *state, const IR::Type_StructLike *type,
                       uint64_t member_id)
    : state(state), member_id(member_id), p4_type(type) {
    width = 0;
    uint64_t flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        const IR::Type *resolved_type = state->resolve_type(field->type);
        P4Z3Instance member_var =
            state->gen_instance(name, resolved_type, flat_id);
        if (auto si = to_type<StructBase>(&member_var)) {
            width += si->get_width();
            flat_id += si->get_member_map()->size();
        } else if (auto tbi = resolved_type->to<IR::Type_Bits>()) {
            width += tbi->width_bits();
            flat_id++;
        } else if (auto tvb = resolved_type->to<IR::Type_Varbits>()) {
            width += tvb->width_bits();
            flat_id++;
        } else if (resolved_type->is<IR::Type_Boolean>()) {
            width++;
            flat_id++;
        } else {
            BUG("Type \"%s\" not supported!.", field->type);
        }
        insert_member(field->name.name, member_var);
        member_types.insert({field->name.name, field->type});
    }
}

StructBase::StructBase(const StructBase &other) : P4ComplexInstance(other) {
    width = other.width;
    state = other.state;
    members.clear();
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance *var = &value_tuple.second;
        if (z3::expr *z3_var = to_type<z3::expr>(var)) {
            z3::expr member_cpy = *z3_var;
            insert_member(name, member_cpy);
        } else if (auto complex_var = to_type<StructInstance>(var)) {
            P4Z3Instance member_cpy = new StructInstance(*complex_var);
            insert_member(name, member_cpy);
        } else if (auto complex_var = to_type<HeaderInstance>(var)) {
            P4Z3Instance member_cpy = new HeaderInstance(*complex_var);
            insert_member(name, member_cpy);
        } else if (auto int_var = to_type<Z3Int>(var)) {
            P4Z3Instance member_cpy = new Z3Int(*int_var);
            insert_member(name, member_cpy);
        } else {
            BUG("Var is neither type z3::expr nor StructBase!");
        }
    }
}
StructBase &StructBase::operator=(const StructBase &other) {
    if (this == &other) {
        return *this;
    }
    width = other.width;
    state = other.state;
    members.clear();
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance *var = &value_tuple.second;
        if (z3::expr *z3_var = to_type<z3::expr>(var)) {
            z3::expr member_cpy = *z3_var;
            insert_member(name, member_cpy);
        } else if (auto complex_var = to_type<StructInstance>(var)) {
            P4Z3Instance member_cpy = new StructInstance(*complex_var);
            insert_member(name, member_cpy);
        } else if (auto complex_var = to_type<HeaderInstance>(var)) {
            P4Z3Instance member_cpy = new HeaderInstance(*complex_var);
            insert_member(name, member_cpy);
        } else if (auto int_var = to_type<Z3Int>(var)) {
            P4Z3Instance member_cpy = new Z3Int(*int_var);
            insert_member(name, member_cpy);
        } else {
            BUG("Var is neither type z3::expr nor StructInstance!");
        }
    }
    return *this;
}

std::vector<std::pair<cstring, z3::expr>>
StructBase::get_z3_vars(cstring prefix) {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        P4Z3Instance *member = &member_tuple.second;
        if (z3::expr *z3_var = to_type<z3::expr>(member)) {
            z3_vars.push_back({name, *z3_var});
        } else if (auto z3_var = to_type<StructBase>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<ErrorInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<EnumInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<Z3Int>(member)) {
            // We receive an int that we need to cast towards the member
            // type
            const IR::Type *type = member_types[member_tuple.first];
            auto val_string = z3_var->val.get_decimal_string(0);
            auto val =
                state->ctx->bv_val(val_string.c_str(), type->width_bits());
            z3_vars.push_back({name, val});
        } else {
            BUG("Var is neither type z3::expr nor P4ComplexInstance!");
        }
    }
    return z3_vars;
}

void StructBase::merge(z3::expr *cond, const P4ComplexInstance *other) {
    auto other_struct = other->to<StructBase>();

    if (!other_struct) {
        BUG("Unsupported merge class.");
    }

    for (auto member_tuple : members) {
        cstring member_name = member_tuple.first;
        P4Z3Instance *then_var = &member_tuple.second;
        auto else_var = other_struct->get_const_member(member_name);
        if (z3::expr *z3_then_var = to_type<z3::expr>(then_var)) {
            z3::expr merged_expr = merge_z3_expr(cond, z3_then_var, else_var);
            update_member(member_name, merged_expr);

        } else if (auto then_complex = to_type<P4ComplexInstance>(then_var)) {
            if (const z3::expr *z3_else_var =
                    to_const_type<z3::expr>(else_var)) {
                then_complex->merge(cond, z3_else_var);
            } else if (auto else_complex =
                           to_const_type<P4ComplexInstance>(else_var)) {
                then_complex->merge(cond, else_complex);
            } else {
                BUG("Z3 complex merge not supported. ");
            }
        }
    }
}

void StructInstance::propagate_validity(z3::expr *valid_expr) {
    for (auto member_tuple : members) {
        P4Z3Instance *member = &member_tuple.second;
        if (auto z3_var = to_type<StructBase>(member)) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

HeaderInstance::HeaderInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : StructBase(state, type, member_id), valid(state->ctx->bool_val(false)) {
    member_functions["setValid"] = std::bind(&HeaderInstance::setValid, this);
    member_functions["setInvalid"] =
        std::bind(&HeaderInstance::setInvalid, this);
    member_functions["isValid"] = std::bind(&HeaderInstance::isValid, this);
}

void HeaderInstance::set_valid(z3::expr *valid_val) { valid = *valid_val; }
const z3::expr *HeaderInstance::get_valid() const { return &valid; }

void HeaderInstance::setValid() { valid = state->ctx->bool_val(true); }

void HeaderInstance::setInvalid() { valid = state->ctx->bool_val(false); }

void HeaderInstance::isValid() { state->set_expr_result(valid); }

void HeaderInstance::propagate_validity(z3::expr *valid_expr) {
    if (valid_expr) {
        valid = *valid_expr;
    } else {
        cstring name = std::to_string(member_id) + "_valid";
        valid = state->ctx->bool_const(name);
        valid_expr = &valid;
    }
    for (auto member_tuple : members) {
        P4Z3Instance *member = &member_tuple.second;
        if (auto z3_var = to_type<StructBase>(member)) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

std::vector<std::pair<cstring, z3::expr>>
HeaderInstance::get_z3_vars(cstring prefix) {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        P4Z3Instance *member = &member_tuple.second;
        if (z3::expr *z3_var = to_type<z3::expr>(member)) {
            const IR::Type *type = member_types[member_tuple.first];
            z3::expr invalid_var = state->gen_z3_expr("invalid", type);
            auto valid_var = z3::ite(valid, *z3_var, invalid_var);
            z3_vars.push_back({name, valid_var});
        } else if (auto z3_var = to_type<StructBase>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<ErrorInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<EnumInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = to_type<Z3Int>(member)) {
            // We receive an int that we need to cast towards the member
            // type
            const IR::Type *type = member_types[member_tuple.first];
            auto val_string = z3_var->val.get_decimal_string(0);
            auto val =
                state->ctx->bv_val(val_string.c_str(), type->width_bits());
            z3_vars.push_back({name, val});
        } else {
            BUG("Var is neither type z3::expr nor P4ComplexInstance!");
        }
    }
    return z3_vars;
}

void HeaderInstance::merge(z3::expr *cond, const P4ComplexInstance *other) {
    auto other_struct = other->to<HeaderInstance>();

    if (!other_struct) {
        BUG("Unsupported merge class.");
    }
    StructBase::merge(cond, other_struct);
    auto valid_merge = z3::ite(*cond, *get_valid(), *other_struct->get_valid());
    set_valid(&valid_merge);
}

EnumInstance::EnumInstance(P4State *state, const IR::Type_Enum *type,
                           uint64_t member_id)
    : state(state), member_id(member_id), p4_type(type) {
    width = 32;
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        insert_member(name, member_var);
    }
}

std::vector<std::pair<cstring, z3::expr>>
EnumInstance::get_z3_vars(cstring prefix) {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    z3::expr z3_const =
        state->ctx->constant(p4_type->name.name, state->ctx->bv_sort(32));
    cstring name = std::to_string(member_id);
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    z3_vars.push_back({name, z3_const});
    return z3_vars;
}

ErrorInstance::ErrorInstance(P4State *state, const IR::Type_Error *type,
                             uint64_t member_id)
    : state(state), member_id(member_id), p4_type(type) {
    width = 32;
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        insert_member(p4_type->name.name, member_var);
    }
}

std::vector<std::pair<cstring, z3::expr>>
ErrorInstance::get_z3_vars(cstring prefix) {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    cstring name = p4_type->name.name;
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    z3::expr z3_const = state->ctx->constant(name, state->ctx->bv_sort(32));
    z3_vars.push_back({std::to_string(member_id), z3_const});
    return z3_vars;
}

ExternInstance::ExternInstance(P4State *, const IR::Type_Extern *type)
    : p4_type(type) {}

// Some print definitions
std::ostream &operator<<(std::ostream &out, const StructBase &instance) {
    out << "{";
    auto member_map = instance.get_immutable_member_map();
    for (auto it = member_map->begin(); it != member_map->end(); ++it) {
        cstring name = it->first;
        P4Z3Instance val = it->second;
        out << name << ": " << val;
        if (std::next(it) != member_map->end()) {
            out << ", ";
        }
    }
    out << "}";
    return out;
}

std::ostream &operator<<(std::ostream &out, const StructBase *instance) {
    return out << *instance;
}

std::ostream &operator<<(std::ostream &out, const Z3Int &instance) {
    out << "Z3Int(" << instance.val << ")";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Z3Int *instance) {
    return out << *instance;
}

std::ostream &operator<<(std::ostream &out, const P4ComplexInstance *type) {
    if (auto si = type->to<StructBase>()) {
        out << si;
    } else if (auto z3_int = type->to<Z3Int>()) {
        out << z3_int;
    } else {
        out << "P4ComplexInstance()";
    }
    return out;
}

} // namespace TOZ3_V2
