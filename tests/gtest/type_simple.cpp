#include "toz3/common/type_simple.h"

#include <z3++.h>

#include <gtest/gtest.h>

#include "ir/ir.h"
#include "toz3/common/state.h"

namespace P4::ToZ3 {
namespace {

TEST(BitvectorAlignment, IntegerOperandsSurviveConversion) {
    z3::context ctx;
    IR::Type_Bits type(8, false);
    Z3Bitvector bits(nullptr, &type, ctx.bv_val(255, 8));
    Z3Int integer(nullptr, ctx.int_val(1));
    const auto *sum = (bits + integer)->to<Z3Bitvector>();
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->get_val()->simplify().get_numeral_uint(), 0U);
    EXPECT_TRUE((bits > integer).simplify().is_true());
    Z3Int equalInteger(nullptr, ctx.int_val(255));
    EXPECT_TRUE((bits == equalInteger).simplify().is_true());
}

TEST(BitvectorMerging, FactoringSharedAddendsPreservesOverflow) {
    z3::context ctx;
    IR::Type_Bits type(8, false);
    const auto base = ctx.bv_const("base", 8);
    const auto condition = ctx.bool_const("condition");
    const auto before = base + ctx.bv_val(255, 8);
    const auto after = base + ctx.bv_val(3, 8);
    Z3Bitvector merged(nullptr, &type, before);
    Z3Bitvector branch(nullptr, &type, after);
    merged.merge(condition, branch);
    z3::solver solver(ctx);
    solver.add(*merged.get_val() != z3::ite(condition, after, before));
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(BitvectorShifts, CountsDoNotWrapAndSignedRightShiftsPreserveTheSign) {
    z3::context ctx;
    const auto *bits = IR::Type_Bits::get(8, true);
    const Z3Bitvector value(nullptr, bits, ctx.bv_val(128, 8), true);
    for (const auto count : {1U, 8U, 256U}) {
        const Z3Bitvector wide(nullptr, IR::Type_Bits::get(32), ctx.bv_val(count, 32));
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
    const auto *bits = IR::Type_Bits::get(16, true);
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
