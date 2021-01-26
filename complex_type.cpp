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
            auto si = boost::any_cast<StructInstance>(member);
            width += si.width;
        } else if (resolved_type->is<IR::Type_Enum>()) {
            auto si = boost::any_cast<EnumInstance>(member);
            width += si.width;
        } else if (resolved_type->is<IR::Type_Error>()) {
            auto si = boost::any_cast<ErrorInstance>(member);
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
        members.emplace(name, member);
        flat_id++;
    }
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
        members.emplace(name, &member_var);
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
        members.emplace(name, &member_var);
    }
}

} // namespace TOZ3_V2
