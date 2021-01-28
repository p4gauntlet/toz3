#include "complex_type.h"

#include <utility>

namespace TOZ3_V2 {

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : state(state), p4_type(type), member_id(member_id) {
    width = 0;
    uint64_t flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        auto resolved_type = state->resolve_type(field->type);
        P4Z3Type member_var = state->gen_instance(name, resolved_type, flat_id);
        if (auto si = check_complex<StructInstance>(member_var)) {
            width += si->width;
        } else if (auto ei = check_complex<EnumInstance>(member_var)) {
            width += ei->width;
        } else if (auto ei = check_complex<ErrorInstance>(member_var)) {
            width += ei->width;
        } else if (auto tbi = resolved_type->to<IR::Type_Bits>()) {
            width += tbi->width_bits();
        } else if (auto tvb = resolved_type->to<IR::Type_Varbits>()) {
            width += tvb->width_bits();
        } else if (resolved_type->is<IR::Type_Boolean>()) {
            width++;
        } else {
            BUG("Type \"%s\" not supported!.", field->type);
        }
        members.insert({field->name.name, member_var});
        flat_id++;
    }
}

std::vector<std::pair<cstring, z3::ast>> StructInstance::get_z3_vars() {
    std::vector<std::pair<cstring, z3::ast>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        P4Z3Type member = member_tuple.second;
        if (z3::ast *z3_var = boost::get<z3::ast>(&member)) {
            z3_vars.push_back({name, *z3_var});
        } else if (auto z3_var = check_complex<StructInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = check_complex<ErrorInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = check_complex<EnumInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else {
            BUG("Var is neither type z3::ast nor P4ComplexInstance!");
        }
    }
    return z3_vars;
}

void StructInstance::bind(z3::ast) {}

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

std::vector<std::pair<cstring, z3::ast>> EnumInstance::get_z3_vars() {
    std::vector<std::pair<cstring, z3::ast>> z3_vars;
    z3::expr z3_const =
        state->ctx->constant(p4_type->name.name, state->ctx->bv_sort(32));
    z3_vars.push_back({std::to_string(member_id), z3_const});
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

std::vector<std::pair<cstring, z3::ast>> ErrorInstance::get_z3_vars() {
    std::vector<std::pair<cstring, z3::ast>> z3_vars;
    z3::expr z3_const =
        state->ctx->constant(p4_type->name.name, state->ctx->bv_sort(32));
    z3_vars.push_back({std::to_string(member_id), z3_const});
    return z3_vars;
}

ExternInstance::ExternInstance(P4State *, const IR::Type_Extern *type)
    : p4_type(type) {}

} // namespace TOZ3_V2
