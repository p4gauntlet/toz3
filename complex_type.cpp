#include "complex_type.h"

namespace TOZ3_V2 {

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : p4_type(type), member_id(member_id) {
    width = 0;
    uint64_t flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        auto resolved_type = state->resolve_type(field->type);
        auto member = state->gen_instance(name, resolved_type);
        if (resolved_type->is<IR::Type_StructLike>()) {
            auto si = boost::get<StructInstance>(member);
            width += si.width;
        } else if (resolved_type->is<IR::Type_Enum>()) {
            auto si = boost::get<EnumInstance>(member);
            width += si.width;
        } else if (resolved_type->is<IR::Type_Error>()) {
            auto si = boost::get<ErrorInstance>(member);
            width += si.width;
        } else if (auto tbi = resolved_type->to<IR::Type_Bits>()) {
            width += tbi->width_bits();
        } else if (auto tvb = resolved_type->to<IR::Type_Varbits>()) {
            width += tvb->width_bits();
        } else if (resolved_type->is<IR::Type_Boolean>()) {
            width++;
        } else {
            BUG("Type \"%s\" not supported!.", field->type);
        }
        members[name] = &member;
        flat_id++;
    }
}

std::vector<z3::ast> StructInstance::get_z3_vars() {
    std::vector<z3::ast> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        auto member = member_tuple.second;
        if (z3::ast *z3_var = boost::get<z3::ast>(member)) {
            z3_vars.push_back(*z3_var);
        } else if (StructInstance *z3_var =
                       boost::get<StructInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        }
    }
    return z3_vars;
}

void StructInstance::bind(z3::ast bind_const) {}

EnumInstance::EnumInstance(P4State *state, const IR::Type_Enum *type)
    : p4_type(type) {
    width = 32;
    z3::sort z3_type = state->ctx->bv_sort(32);
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        members[name] = &member_var;
    }
}

ErrorInstance::ErrorInstance(P4State *state, const IR::Type_Error *type)
    : p4_type(type) {
    width = 32;
    z3::sort z3_type = state->ctx->bv_sort(32);
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        members[name] = &member_var;
    }
}

ExternInstance::ExternInstance(P4State *state, const IR::Type_Extern *type)
    : p4_type(type) {}

} // namespace TOZ3_V2
