#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"

namespace P4::ToZ3 {
namespace {

TEST(Enums, ExtendedNamespacesHaveDistinctLabelsBeyondSixteenMembers) {
    z3::context ctx;
    P4State state(&ctx);
    const IR::Type_Enum type(IR::ID("E"_cs), {new IR::Declaration_ID("First"_cs)});
    EnumInstance instance(&state, &type, "e"_cs, 0);
    for (unsigned i = 1; i < 24; ++i) {
        const cstring name = std::to_string(i);
        instance.add_enum_member(name);
        instance.add_enum_member(name);
        const auto *label = instance.get_member(name)->to<NumericVal>();
        ASSERT_NE(label, nullptr);
        EXPECT_EQ(label->get_val()->get_sort().bv_size(), 32U);
        EXPECT_EQ(label->get_val()->get_numeral_uint(), i);
    }
}

TEST(SerializedEnums, UnderlyingValuesPreserveWidthSignednessAndSymmetricEquality) {
    z3::context ctx;
    P4State state(&ctx);
    for (const bool isSigned : {false, true}) {
        const auto *bits = IR::Type_Bits::get(8, isSigned);
        const IR::Type_SerEnum type(IR::ID("E"_cs), bits, {});
        SerEnumInstance value(&state, {}, &type, "e"_cs, 0);
        const auto symbolic = ctx.bv_const("e", 8);
        value.set_enum_val(symbolic);
        const Z3Bitvector numeric(&state, bits, symbolic, isSigned);
        const auto *cast = value.cast_allocate(bits)->to<Z3Bitvector>();
        ASSERT_NE(cast, nullptr);
        EXPECT_EQ(cast->bv_is_signed(), isSigned);
        EXPECT_EQ(cast->get_val()->get_sort().bv_size(), 8U);
        z3::solver solver(ctx);
        solver.add((*cast->get_val() != symbolic) || !(value == numeric) || !(numeric == value));
        EXPECT_EQ(solver.check(), z3::unsat);
        const Z3Bitvector negative(&state, bits, ctx.bv_val(255, 8), isSigned);
        const auto *instantiated = value.instantiate(negative);
        const auto *underlying = instantiated->cast_allocate(bits)->to<Z3Bitvector>();
        ASSERT_NE(underlying, nullptr);
        const auto *wide = underlying->cast_allocate(IR::Type_Bits::get(16))->to<Z3Bitvector>();
        ASSERT_NE(wide, nullptr);
        EXPECT_EQ(wide->get_val()->simplify().get_numeral_uint(), isSigned ? 65535U : 255U);
    }
}

}  // namespace
}  // namespace P4::ToZ3
