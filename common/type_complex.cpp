#include "type_complex.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

StructBase::StructBase(P4State *state, const IR::Type_StructLike *type,
                       uint64_t member_id)
    : state(state), member_id(member_id), p4_type(type) {
    width = 0;
    uint64_t flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        const IR::Type *resolved_type = state->resolve_type(field->type);
        P4Z3Instance *member_var =
            state->gen_instance(name, resolved_type, flat_id);
        if (auto si = member_var->to_mut<StructBase>()) {
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

StructBase::StructBase(const StructBase &other) : P4Z3Instance(other) {
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance *member_cpy = value_tuple.second->copy();
        state->add_to_allocated_vars(member_cpy);
        insert_member(name, member_cpy);
    }
}

StructBase &StructBase::operator=(const StructBase &other) {
    if (this == &other) {
        return *this;
    }
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance *member_cpy = value_tuple.second->copy();
        state->add_to_allocated_vars(member_cpy);
        insert_member(name, member_cpy);
    }
    return *this;
}

std::vector<std::pair<cstring, z3::expr>>
StructBase::get_z3_vars(cstring prefix) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        P4Z3Instance *member = member_tuple.second;
        if (auto z3_var = member->to<Z3Bitvector>()) {
            z3_vars.push_back({name, z3_var->val});
        } else if (auto z3_var = member->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = member->to<Z3Int>()) {
            // We receive an int that we need to cast towards the member type
            // Ints must be compile constants so we can resolve them
            const IR::Type *dest_type = member_types.at(member_tuple.first);
            auto cast_val =
                z3::int2bv(dest_type->width_bits(), z3_var->val).simplify();
            z3_vars.push_back({name, cast_val});
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }
    return z3_vars;
}

void StructBase::merge(z3::expr *cond, const P4Z3Instance *other) {
    auto other_struct = other->to<StructBase>();

    if (!other_struct) {
        BUG("Unsupported merge class.");
    }

    for (auto member_tuple : members) {
        cstring member_name = member_tuple.first;
        P4Z3Instance *then_var = member_tuple.second;
        auto else_var = other_struct->get_const_member(member_name);
        then_var->merge(cond, else_var);
    }
}

void StructInstance::propagate_validity(z3::expr *valid_expr) {
    for (auto member_tuple : members) {
        P4Z3Instance *member = member_tuple.second;
        if (auto z3_var = member->to_mut<StructBase>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

StructInstance *StructInstance::copy() const {
    return new StructInstance(*this);
};

HeaderInstance::HeaderInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : StructBase(state, type, member_id),
      valid(state->get_z3_ctx()->bool_val(false)) {
    member_functions["setValid"] = std::bind(&HeaderInstance::setValid, this);
    member_functions["setInvalid"] =
        std::bind(&HeaderInstance::setInvalid, this);
    member_functions["isValid"] = std::bind(&HeaderInstance::isValid, this);
}

void HeaderInstance::set_valid(z3::expr *valid_val) { valid = *valid_val; }
const z3::expr *HeaderInstance::get_valid() const { return &valid; }

void HeaderInstance::setValid() { valid = state->get_z3_ctx()->bool_val(true); }

void HeaderInstance::setInvalid() {
    valid = state->get_z3_ctx()->bool_val(false);
}

void HeaderInstance::isValid() {
    static Z3Bitvector wrapper = Z3Bitvector(valid);
    state->set_expr_result(wrapper);
}

void HeaderInstance::propagate_validity(z3::expr *valid_expr) {
    if (valid_expr) {
        valid = *valid_expr;
    } else {
        cstring name = std::to_string(member_id) + "_valid";
        valid = state->get_z3_ctx()->bool_const(name);
        valid_expr = &valid;
    }
    for (auto member_tuple : members) {
        P4Z3Instance *member = member_tuple.second;
        if (auto z3_var = member->to_mut<StructBase>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

HeaderInstance *HeaderInstance::copy() const {
    return new HeaderInstance(*this);
};

std::vector<std::pair<cstring, z3::expr>>
HeaderInstance::get_z3_vars(cstring prefix) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        P4Z3Instance *member = member_tuple.second;
        if (auto *z3_var = member->to<Z3Bitvector>()) {
            const IR::Type *type = member_types.at(member_tuple.first);
            z3::expr invalid_var = state->gen_z3_expr("invalid", type);
            auto valid_var = z3::ite(valid, z3_var->val, invalid_var);
            z3_vars.push_back({name, valid_var});
        } else if (auto z3_var = member->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = member->to<Z3Int>()) {
            // We receive an int that we need to cast towards the member type
            const IR::Type *dest_type = member_types.at(member_tuple.first);
            auto cast_val =
                z3::int2bv(dest_type->width_bits(), z3_var->val).simplify();
            printf("SIMPLIFIED VAL %s", cast_val.to_string().c_str());
            z3::expr invalid_var = state->gen_z3_expr("invalid", dest_type);
            auto valid_var = z3::ite(valid, cast_val, invalid_var);
            z3_vars.push_back({name, valid_var});
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }
    return z3_vars;
}

void HeaderInstance::merge(z3::expr *cond, const P4Z3Instance *other) {
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
EnumInstance::get_z3_vars(cstring prefix) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    z3::expr z3_const = state->get_z3_ctx()->constant(
        p4_type->name.name, state->get_z3_ctx()->bv_sort(32));
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
ErrorInstance::get_z3_vars(cstring prefix) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    cstring name = p4_type->name.name;
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    z3::expr z3_const =
        state->get_z3_ctx()->constant(name, state->get_z3_ctx()->bv_sort(32));
    z3_vars.push_back({std::to_string(member_id), z3_const});
    return z3_vars;
}

ErrorInstance *ErrorInstance::copy() const {
    return new ErrorInstance(state, p4_type, member_id);
};

ExternInstance::ExternInstance(P4State *, const IR::Type_Extern *type)
    : p4_type(type) {
    for (auto method : type->methods) {
        methods.insert({method->getName().name, method});
    }
}

} // namespace TOZ3_V2
