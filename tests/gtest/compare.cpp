#include "toz3/compare/compare.h"

#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"

namespace P4::ToZ3 {
namespace {

constexpr unsigned SHARED_DAG_DEPTH = 40;
constexpr unsigned COMPARED_FIELD_WIDTH = 8;

TEST(Definedness, HandlesDeeplySharedExpressionsWithoutExpandingTheTree) {
    z3::context ctx;
    const auto combine = ctx.function("combine", ctx.bv_sort(8), ctx.bv_sort(8), ctx.bv_sort(8));
    auto expression = ctx.bv_const("input", COMPARED_FIELD_WIDTH);
    // Only 41 distinct nodes, but expanding this DAG as a tree takes over 2^40 visits.
    for (unsigned i = 0; i < SHARED_DAG_DEPTH; ++i) {
        expression = combine(expression, expression);
    }
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
    solver.add(defined_when(before) && before != ctx.bv_val(3, COMPARED_FIELD_WIDTH));
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
    solver.add(defined_when(before) && valid && before != ctx.bv_val(2, COMPARED_FIELD_WIDTH));
    EXPECT_EQ(solver.check(), z3::sat);
    solver.reset();
    solver.add(defined_when(unknown || valid) != valid);
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST(Definedness, InvalidHeaderOutputFieldsAreUndefinedButValidityRemainsDefined) {
    z3::context ctx;
    P4State state(&ctx);
    const auto bits = IR::Type_Bits::get(COMPARED_FIELD_WIDTH);
    const IR::Type_Header type(IR::ID("H"_cs), {new IR::StructField("field"_cs, bits)});
    HeaderInstance header(&state, &type, "input"_cs, 0);
    const auto valid = ctx.bool_const("valid");
    header.set_valid(valid);
    const auto input = ctx.bv_const("input", COMPARED_FIELD_WIDTH);
    header.bind(&input, COMPARED_FIELD_WIDTH);
    const auto outputs = header.get_z3_vars("out"_cs);
    ASSERT_EQ(outputs.size(), 2U);
    EXPECT_EQ(outputs[0].first, "out.$valid"_cs);
    EXPECT_TRUE(defined_when(outputs[0].second).is_true());
    const auto &field = outputs[1].second;
    z3::solver solver(ctx);
    solver.add(defined_when(field) != valid);
    EXPECT_EQ(solver.check(), z3::unsat);

    // Strict comparison still sees arbitrary invalid-field values, while the
    // definedness filter tolerates only mismatches on invalid paths.
    const auto mismatch = field != ctx.bv_val(1, COMPARED_FIELD_WIDTH);
    solver.reset();
    solver.add(!valid && mismatch);
    EXPECT_EQ(solver.check(), z3::sat);
    solver.add(defined_when(field));
    EXPECT_EQ(solver.check(), z3::unsat);
    solver.reset();
    solver.add(valid && defined_when(field) && mismatch);
    EXPECT_EQ(solver.check(), z3::sat);
}

TEST(Definedness, InvalidSerializedEnumOutputsUseTheSameUndefinedMarker) {
    z3::context ctx;
    P4State state(&ctx);
    const IR::Type_SerEnum type(IR::ID("E"_cs), IR::Type_Bits::get(COMPARED_FIELD_WIDTH), {});
    SerEnumInstance value(&state, {}, &type, "field"_cs, 0);
    value.set_enum_val(ctx.bv_const("input", COMPARED_FIELD_WIDTH));
    const auto valid = ctx.bool_const("valid");
    const auto field = value.get_z3_vars("out"_cs, &valid).front().second;
    z3::solver solver(ctx);
    solver.add(defined_when(field) != valid);
    EXPECT_EQ(solver.check(), z3::unsat);
}

}  // namespace
}  // namespace P4::ToZ3
