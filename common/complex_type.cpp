#include "complex_type.h"

#include <cstdio>
#include <utility>

#include "scope.h"
#include "z3_int.h"

namespace TOZ3_V2 {

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : state(state), p4_type(type), member_id(member_id) {
    width = 0;
    uint64_t flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        const IR::Type *resolved_type = state->resolve_type(field->type);
        P4Z3Instance member_var =
            state->gen_instance(name, resolved_type, flat_id);
        if (auto si = check_complex<StructInstance>(member_var)) {
            width += si->width;
            flat_id += si->members.size();
        } else if (auto ei = check_complex<EnumInstance>(member_var)) {
            width += ei->width;
            flat_id += ei->members.size();
        } else if (auto ei = check_complex<ErrorInstance>(member_var)) {
            width += ei->width;
            flat_id += ei->members.size();
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
        members.insert({field->name.name, member_var});
        member_types.insert({field->name.name, field->type});
    }
}

StructInstance::StructInstance(const StructInstance &other)
    : P4ComplexInstance(other) {
    width = other.width;
    state = other.state;
    members.clear();
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance var = value_tuple.second;
        if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
            z3::expr member_cpy = *z3_var;
            members.insert({name, member_cpy});
        } else if (auto complex_var = check_complex<StructInstance>(var)) {
            P4Z3Instance member_cpy = new StructInstance(*complex_var);
            members.insert({name, member_cpy});
        } else if (auto int_var = check_complex<Z3Int>(var)) {
            P4Z3Instance member_cpy = new Z3Int(*int_var);
            members.insert({name, member_cpy});
        } else {
            BUG("Var is neither type z3::expr nor StructInstance!");
        }
    }
}

StructInstance &StructInstance::operator=(const StructInstance &other) {
    if (this == &other) {
        return *this;
    }
    width = other.width;
    state = other.state;
    members.clear();
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        P4Z3Instance var = value_tuple.second;
        if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
            z3::expr member_cpy = *z3_var;
            members.insert({name, member_cpy});
        } else if (auto complex_var = check_complex<StructInstance>(var)) {
            P4Z3Instance member_cpy = new StructInstance(*complex_var);
            members.insert({name, member_cpy});
        } else if (auto int_var = check_complex<Z3Int>(var)) {
            P4Z3Instance member_cpy = new Z3Int(*int_var);
            members.insert({name, member_cpy});
        } else {
            BUG("Var is neither type z3::expr nor StructInstance!");
        }
    }
    return *this;
}

std::vector<std::pair<cstring, z3::expr>>
StructInstance::get_z3_vars(cstring prefix) {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        P4Z3Instance member = member_tuple.second;
        if (z3::expr *z3_var = boost::get<z3::expr>(&member)) {
            z3_vars.push_back({name, *z3_var});
        } else if (auto z3_var = check_complex<StructInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = check_complex<ErrorInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = check_complex<EnumInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars(name);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = check_complex<Z3Int>(member)) {
            // We receive an int that we need to cast towards the member type
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

void StructInstance::bind(z3::expr) {}

EnumInstance::EnumInstance(P4State *state, const IR::Type_Enum *type,
                           uint64_t member_id)
    : state(state), p4_type(type), member_id(member_id) {
    width = 32;
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        members.insert({name, member_var});
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
    : state(state), p4_type(type), member_id(member_id) {
    width = 32;
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        members.insert({p4_type->name.name, member_var});
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

} // namespace TOZ3_V2
