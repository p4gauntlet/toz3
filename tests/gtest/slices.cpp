#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

TEST(WidthSlices, ZeroWidthHasOnlyZeroValue) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    IR::PlusSlice slice(new IR::Constant(IR::Type_Bits::get(8), 214), new IR::Constant(3), 0);
    visitor.visit(&slice);
    EXPECT_EQ(state.get_expr_result<NumericVal>()->get_val()->get_numeral_uint(), 0U);
}

TEST(WidthSlices, SymbolicOffsetsDoNotWrapOrSignExtend) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    const auto *offsetType = IR::Type_Bits::get(32);
    auto offset = ctx.bv_const("offset", 32);
    state.declare_var("offset"_cs, new Z3Bitvector(&state, offsetType, offset), offsetType);
    IR::PlusSlice slice(new IR::Constant(IR::Type_Bits::get(8), 214),
                        new IR::PathExpression("offset"_cs), 4);
    visitor.visit(&slice);
    const auto result = *state.get_expr_result<NumericVal>()->get_val();
    auto expected = ctx.bv_val(0, 4);
    for (unsigned idx = 0; idx < 8; ++idx) {
        expected =
            z3::ite(offset == ctx.bv_val(idx, 32), ctx.bv_val((214U >> idx) & 15U, 4), expected);
    }
    z3::solver solver(ctx);
    solver.add(result != expected);
    EXPECT_EQ(solver.check(), z3::unsat);
}

}  // namespace
}  // namespace P4::ToZ3
