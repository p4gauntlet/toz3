#include "toz3/compare/compare.h"

#include <gtest/gtest.h>

namespace P4::ToZ3 {
namespace {

TEST(Definedness, HandlesDeeplySharedExpressionsWithoutExpandingTheTree) {
    z3::context ctx;
    const auto combine = ctx.function("combine", ctx.bv_sort(8), ctx.bv_sort(8), ctx.bv_sort(8));
    auto expression = ctx.bv_const("input", 8);
    // Only 41 distinct nodes, but expanding this DAG as a tree takes over 2^40 visits.
    for (unsigned i = 0; i < 40; ++i) expression = combine(expression, expression);
    EXPECT_TRUE(defined_when(expression).is_true());
}

TEST(Definedness, UndefinedBranchesPreserveTheGuardForDefinedInputs) {
    z3::context ctx;
    const auto combine = ctx.function("combine", ctx.bv_sort(8), ctx.bv_sort(8), ctx.bv_sort(8));
    const auto partial =
        z3::ite(ctx.bool_const("condition"), ctx.bv_const("p4z3_undefined", 8), ctx.bv_val(1, 8));
    const auto result = defined_when(combine(partial, partial));
    z3::solver solver(ctx);
    solver.add(result != !ctx.bool_const("condition"));
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(Definedness, ConditionalInvalidHeaderReadsDoNotHideDefinedViolations) {
    z3::context ctx;
    const auto valid = ctx.bool_const("valid");
    const auto input = ctx.bv_const("input", 8);
    const auto field = z3::ite(valid, input, ctx.bv_const("p4z3_undefined", 8));
    const auto before = z3::ite(field == ctx.bv_val(1, 8), ctx.bv_val(2, 8), ctx.bv_val(3, 8));
    z3::solver solver(ctx);
    solver.add(defined_when(before) && before != ctx.bv_val(3, 8));
    ASSERT_EQ(solver.check(), z3::sat);
    EXPECT_TRUE(solver.get_model().eval(valid).is_true());
    solver.add(!valid);
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(Definedness, MixedWidthOperationsKeepTheirOriginalSorts) {
    z3::context ctx;
    const auto valid = ctx.bool_const("valid");
    const auto byte = z3::ite(valid, ctx.bv_val(127, 8), ctx.bv_const("p4z3_undefined", 8));
    const auto bit = byte.extract(0, 0);
    const auto result = z3::ite(bit == ctx.bv_val(1, 1), byte + 1, ctx.bv_val(0, 8));
    z3::solver solver(ctx);
    solver.add(defined_when(result) != valid);
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(Definedness, SimplifiedBooleanGuardsStillMaskUndefinedReads) {
    z3::context ctx;
    const auto valid = ctx.bool_const("valid");
    const auto unknown = ctx.bv_const("p4z3_undefined", 8) == ctx.bv_val(1, 8);
    const auto before = z3::ite(
        valid, z3::ite(unknown && !valid, ctx.bv_val(2, 8), ctx.bv_val(1, 8)), ctx.bv_val(0, 8));
    z3::solver solver(ctx);
    solver.add(defined_when(before) && valid && before != ctx.bv_val(2, 8));
    EXPECT_EQ(solver.check(), z3::sat);
    solver.reset();
    solver.add(defined_when(unknown || valid) != valid);
    EXPECT_EQ(solver.check(), z3::unsat);
}

}  // namespace
}  // namespace P4::ToZ3
