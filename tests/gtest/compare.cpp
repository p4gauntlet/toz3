#include "toz3/compare/compare.h"

#include <gtest/gtest.h>

namespace P4::ToZ3 {
namespace {

TEST(TaintAnalysis, PreservesDeeplySharedDefinedExpressions) {
    z3::context ctx;
    const auto combine = ctx.function("combine", ctx.bv_sort(8), ctx.bv_sort(8), ctx.bv_sort(8));
    auto expression = ctx.bv_const("input", 8);
    // Only 41 distinct nodes, but expanding this DAG as a tree takes over 2^40 visits.
    for (unsigned i = 0; i < 40; ++i) expression = combine(expression, expression);
    std::set<z3::expr> taint;
    EXPECT_TRUE(z3::eq(substitute_taint(&ctx, expression, &taint), expression));
    EXPECT_TRUE(taint.empty());
}

TEST(TaintAnalysis, SharedUndefinedOccurrencesKeepIndependentFreshSymbols) {
    z3::context ctx;
    const auto combine = ctx.function("combine", ctx.bv_sort(8), ctx.bv_sort(8), ctx.bv_sort(8));
    const auto partial =
        z3::ite(ctx.bool_const("condition"), ctx.bv_const("p4z3_undefined", 8), ctx.bv_val(1, 8));
    std::set<z3::expr> taint;
    const auto result = substitute_taint(&ctx, combine(partial, partial), &taint);
    ASSERT_EQ(taint.size(), 2U);
    ASSERT_TRUE(result.arg(0).is_ite());
    ASSERT_TRUE(result.arg(1).is_ite());
    EXPECT_FALSE(z3::eq(result.arg(0).arg(1), result.arg(1).arg(1)));
    EXPECT_TRUE(z3::eq(result.arg(0).arg(2), ctx.bv_val(1, 8)));
    EXPECT_TRUE(z3::eq(result.arg(1).arg(2), ctx.bv_val(1, 8)));
}

}  // namespace
}  // namespace P4::ToZ3
