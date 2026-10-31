#include "toz3/common/type_simple.h"

#include <z3++.h>

#include <gtest/gtest.h>

#include "ir/ir.h"
#include "toz3/common/state.h"

namespace P4::ToZ3 {
namespace {

constexpr int BITVECTOR_WIDTH = 8;
constexpr int WIDE_WIDTH = 16;
constexpr int BYTE_MAXIMUM = 255;
constexpr int BYTE_SIGN_BIT = 128;

TEST(BitvectorAlignment, IntegerOperandsSurviveConversion) {
    z3::context ctx;
    IR::Type_Bits type(BITVECTOR_WIDTH, false);
    Z3Bitvector bits(nullptr, &type, ctx.bv_val(BYTE_MAXIMUM, BITVECTOR_WIDTH));
    Z3Int integer(nullptr, ctx.int_val(1));
    const auto *sum = (bits + integer)->to<Z3Bitvector>();
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->get_val()->simplify().get_numeral_uint(), 0U);
    EXPECT_TRUE((bits > integer).simplify().is_true());
    Z3Int equalInteger(nullptr, ctx.int_val(BYTE_MAXIMUM));
    EXPECT_TRUE(bits.operator==(equalInteger).simplify().is_true());
    EXPECT_TRUE(equalInteger.operator==(bits).simplify().is_true());
    EXPECT_TRUE(integer.operator!=(bits).simplify().is_true());
}

TEST(BitvectorMerging, FactoringSharedAddendsPreservesOverflow) {
    z3::context ctx;
    IR::Type_Bits type(BITVECTOR_WIDTH, false);
    const auto base = ctx.bv_const("base", BITVECTOR_WIDTH);
    const auto condition = ctx.bool_const("condition");
    const auto before = base + ctx.bv_val(BYTE_MAXIMUM, BITVECTOR_WIDTH);
    const auto after = base + ctx.bv_val(3, BITVECTOR_WIDTH);
    Z3Bitvector merged(nullptr, &type, before);
    Z3Bitvector branch(nullptr, &type, after);
    merged.merge(condition, branch);
    z3::solver solver(ctx);
    solver.add(*merged.get_val() != z3::ite(condition, after, before));
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(BitvectorShifts, CountsDoNotWrapAndSignedRightShiftsPreserveTheSign) {
    z3::context ctx;
    const auto bits = IR::Type_Bits::get(BITVECTOR_WIDTH, true);
    const Z3Bitvector value(nullptr, bits, ctx.bv_val(BYTE_SIGN_BIT, BITVECTOR_WIDTH), true);
    for (const auto count : {1U, 8U, 256U}) {
        const Z3Bitvector wide(nullptr, IR::Type_Bits::get(INT_WIDTH),
                               ctx.bv_val(count, INT_WIDTH));
        const auto *right = (value >> wide)->to<NumericVal>();
        EXPECT_EQ(right->get_val()->simplify().get_numeral_uint(), count == 1 ? 192U : 255U);
        const auto *left = (value << wide)->to<NumericVal>();
        EXPECT_EQ(left->get_val()->simplify().get_numeral_uint(), 0U);
    }
    const Z3Int enormous(nullptr, ctx.int_val("18446744073709551616"));
    EXPECT_EQ((value >> enormous)->to<NumericVal>()->get_val()->simplify().get_numeral_uint(),
              255U);
    EXPECT_EQ((value << enormous)->to<NumericVal>()->get_val()->simplify().get_numeral_uint(), 0U);
}

TEST(BitvectorAllocation, DeclaredSignednessSurvivesAggregateBinding) {
    z3::context ctx;
    P4State state(&ctx);
    const auto bits = IR::Type_Bits::get(WIDE_WIDTH, true);
    const auto *value = state.gen_instance("signed"_cs, bits)->to<Z3Bitvector>();
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(value->bv_is_signed());
    const IR::Type_Header header(IR::ID("H"_cs), {new IR::StructField("x"_cs, bits)});
    HeaderInstance instance(&state, &header, "header"_cs, 0);
    instance.bind(nullptr, 0);
    const auto *field = instance.get_member("x"_cs)->to<Z3Bitvector>();
    ASSERT_NE(field, nullptr);
    EXPECT_TRUE(field->bv_is_signed());
}

}  // namespace
}  // namespace P4::ToZ3
