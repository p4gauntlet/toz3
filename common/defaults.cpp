#include "defaults.h"

#include "exceptions.h"
#include "state.h"

namespace P4::ToZ3 {
P4Z3Instance *DefaultInstance::cast_allocate(const IR::Type *type) const {
    type = state->resolve_type(type);
    auto *ctx = state->get_z3_ctx();
    if (type->is<IR::Type_InfInt>()) return new Z3Int(state, ctx->int_val(0));
    if (type->is<IR::Type_String>()) return new Z3Bitvector(state, type, ctx->string_val(""));
    if (type->is<IR::Type_Boolean>()) return new Z3Bitvector(state, type, ctx->bool_val(false));
    if (const auto *bits = type->to<IR::Type_Bits>()) {
        return new Z3Bitvector(state, type,
                               bits->size == 0 ? ctx->int_val(0) : ctx->bv_val(0, bits->size),
                               bits->isSigned);
    }
    if (const auto *enumeration = type->to<IR::Type_Enum>()) {
        const auto *definition = state->get_var<EnumInstance>(enumeration->name.name);
        const auto *first = definition->get_member(enumeration->members.at(0)->name);
        auto *result = definition->copy();
        result->set_enum_val(*first->to<ValContainer>()->get_val());
        return result;
    }
    if (const auto *error = type->to<IR::Type_Error>()) {
        const auto *definition = state->get_var<ErrorInstance>(error->name.name);
        auto *result = definition->copy();
        result->set_enum_val(*definition->get_member("NoError"_cs)->to<ValContainer>()->get_val());
        return result;
    }
    if (const auto *enumeration = type->to<IR::Type_SerEnum>()) {
        const auto *definition = state->get_var<SerEnumInstance>(enumeration->name.name);
        return definition->instantiate(Z3Int(state, ctx->int_val(0)));
    }
    auto *result = state->gen_instance("default"_cs, type);
    if (auto *header = result->to_mut<HeaderInstance>()) {
        header->set_valid(ctx->bool_val(false));
        return header;
    }
    if (auto *aggregate = result->to_mut<StructBase>()) {
        for (const auto &entry : *aggregate->get_member_map()) {
            const auto *member_type = aggregate->get_member_type(entry.first);
            aggregate->update_member(entry.first, cast_allocate(member_type));
        }
        return result;
    }
    throw UnsupportedFeatureError("Default value is undefined for type " + type->toString());
}
}  // namespace P4::ToZ3
