#include "toz3/common/type_simple.h"

#include <z3++.h>

#include <gtest/gtest.h>

#include "ir/ir.h"

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

}  // namespace
}  // namespace P4::ToZ3
