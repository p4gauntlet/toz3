#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

constexpr int SOURCE_WIDTH = 8;
constexpr unsigned SOURCE_VALUE = 214;
constexpr int OFFSET_WIDTH = 32;
constexpr unsigned SLICE_WIDTH = 4;
constexpr unsigned SLICE_MASK = (1U << SLICE_WIDTH) - 1;

TEST(WidthSlices, ZeroWidthHasOnlyZeroValue) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    IR::PlusSlice slice(new IR::Constant(IR::Type_Bits::get(SOURCE_WIDTH), SOURCE_VALUE),
                        new IR::Constant(3), 0);
    visitor.visit(&slice);
    EXPECT_EQ(state.get_expr_result<NumericVal>()->get_val()->get_numeral_uint(), 0U);
}

TEST(WidthSlices, SymbolicOffsetsDoNotWrapOrSignExtend) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    const auto *offsetType = IR::Type_Bits::get(OFFSET_WIDTH);
    auto offset = ctx.bv_const("offset", OFFSET_WIDTH);
    state.declare_var("offset"_cs,
                      allocate_instance<Z3Bitvector>(&state, &state, offsetType, offset),
                      offsetType);
    IR::PlusSlice slice(new IR::Constant(IR::Type_Bits::get(SOURCE_WIDTH), SOURCE_VALUE),
                        new IR::PathExpression("offset"_cs), SLICE_WIDTH);
    visitor.visit(&slice);
    const auto result = *state.get_expr_result<NumericVal>()->get_val();
    auto expected = ctx.bv_val(0, SLICE_WIDTH);
    for (unsigned idx = 0; idx < SOURCE_WIDTH; ++idx) {
        expected = z3::ite(offset == ctx.bv_val(idx, OFFSET_WIDTH),
                           ctx.bv_val((SOURCE_VALUE >> idx) & SLICE_MASK, SLICE_WIDTH), expected);
    }
    z3::solver solver(ctx);
    solver.add(result != expected);
    EXPECT_EQ(solver.check(), z3::unsat);
}

}  // namespace
}  // namespace P4::ToZ3
