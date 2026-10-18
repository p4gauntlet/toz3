#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"

namespace P4::ToZ3 {
namespace {

constexpr int ENUM_WIDTH = 8;
constexpr int WIDE_ENUM_WIDTH = 16;
constexpr unsigned EXTENDED_LABEL_COUNT = 24;

TEST(Enums, ExtendedNamespacesHaveDistinctLabelsBeyondSixteenMembers) {
    z3::context ctx;
    P4State state(&ctx);
    const IR::Type_Enum type(IR::ID("E"_cs), {new IR::Declaration_ID("First"_cs)});
    EnumInstance instance(&state, &type, "e"_cs, 0);
    for (unsigned i = 1; i < EXTENDED_LABEL_COUNT; ++i) {
        const cstring name = std::to_string(i);
        instance.add_enum_member(name);
        instance.add_enum_member(name);
        const auto *label = instance.get_member(name)->to<NumericVal>();
        ASSERT_NE(label, nullptr);
        EXPECT_EQ(label->get_val()->get_sort().bv_size(), 32U);
        EXPECT_EQ(label->get_val()->get_numeral_uint(), i);
    }
}

void checkEnumWidening(P4State *state, const SerEnumInstance &value, const IR::Type_Bits *bits,
                       bool isSigned) {
    const Z3Bitvector negative(state, bits, state->get_z3_ctx()->bv_val(-1, ENUM_WIDTH), isSigned);
    const auto *instantiated = value.instantiate(negative);
    const auto *underlying = instantiated->cast_allocate(bits)->to<Z3Bitvector>();
    ASSERT_NE(underlying, nullptr);
    const auto *wide =
        underlying->cast_allocate(IR::Type_Bits::get(WIDE_ENUM_WIDTH))->to<Z3Bitvector>();
    ASSERT_NE(wide, nullptr);
    EXPECT_EQ(wide->get_val()->simplify().get_numeral_uint(), isSigned ? 65535U : 255U);
}

void checkSerializedEnum(bool isSigned) {
    z3::context ctx;
    P4State state(&ctx);
    const auto *bits = IR::Type_Bits::get(ENUM_WIDTH, isSigned);
    const IR::Type_SerEnum type(IR::ID("E"_cs), bits, {});
    SerEnumInstance value(&state, {}, &type, "e"_cs, 0);
    const auto symbolic = ctx.bv_const("e", ENUM_WIDTH);
    value.set_enum_val(symbolic);
    const Z3Bitvector numeric(&state, bits, symbolic, isSigned);
    const auto *cast = value.cast_allocate(bits)->to<Z3Bitvector>();
    ASSERT_NE(cast, nullptr);
    EXPECT_EQ(cast->bv_is_signed(), isSigned);
    EXPECT_EQ(cast->get_val()->get_sort().bv_size(), 8U);
    z3::solver solver(ctx);
    solver.add((*cast->get_val() != symbolic) || !value.operator==(numeric) ||
               !numeric.operator==(value));
    EXPECT_EQ(solver.check(), z3::unsat);
    checkEnumWidening(&state, value, bits, isSigned);
}

TEST(SerializedEnums, UnderlyingValuesPreserveWidthSignednessAndSymmetricEquality) {
    checkSerializedEnum(false);
    checkSerializedEnum(true);
}

}  // namespace
}  // namespace P4::ToZ3
