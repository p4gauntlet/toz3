#include <gtest/gtest.h>

#include "frontends/common/parseInput.h"
#include "interpreter_state.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/exceptions.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

class TypeInferenceTest : public ::testing::Test, protected InterpreterTestState {
 protected:
    TypeInferenceTest() { state.push_scope(); }

    void evaluate(const char *source) {
        const auto program = parseP4String(source);
        ASSERT_NE(program, nullptr);
        visitor.visit(program);
        const auto *control = state.get_type("C"_cs)->checkedTo<IR::P4Control>();
        for (const auto &local : control->controlLocals) {
            visitor.visit(local);
        }
        visitor.visit(control->body);
    }
    unsigned value(cstring name) {
        return state.get_var<NumericVal>(name)->get_val()->simplify().get_numeral_uint();
    }
    void rejectsWidths(int left, int right) {
        const IR::MethodCallExpression call(new IR::PathExpression("first"_cs),
                                            {new IR::Constant(IR::Type_Bits::get(left), 1),
                                             new IR::Constant(IR::Type_Bits::get(right), 2)});
        Z3Visitor callVisitor(&state);
        EXPECT_THROW(callVisitor.visit(&call), UnsupportedFeatureError);
    }
};

TEST_F(TypeInferenceTest, AllArgumentsDetermineTheTypeBeforeCasting) {
    evaluate(R"(
        T add<T>(in T left, in T right) { return left + right; }
        control C() {
            bit<16> first = add(255, 8w1);
            bit<16> last = add(8w1, 255);
            bit<16> explicitType = add<bit<16>>(8w1, 255);
            apply {}
        }
    )");
    EXPECT_EQ(value("first"_cs), 0U);
    EXPECT_EQ(value("last"_cs), 0U);
    EXPECT_EQ(value("explicitType"_cs), 256U);
    EXPECT_EQ(state.check_for_type("T"_cs), nullptr);
}

TEST_F(TypeInferenceTest, ConflictingSizedArgumentsAreRejectedInEitherOrder) {
    evaluate(R"(
        T first<T>(in T left, in T right) { return left; }
        control C() { apply {} }
    )");
    constexpr int NARROW_WIDTH = 8;
    constexpr int WIDE_WIDTH = 16;
    rejectsWidths(NARROW_WIDTH, WIDE_WIDTH);
    rejectsWidths(WIDE_WIDTH, NARROW_WIDTH);
    EXPECT_EQ(state.check_for_type("T"_cs), nullptr);
}

TEST_F(TypeInferenceTest, DiscardedOutputGetsItsTypeFromALaterArgument) {
    evaluate(R"(
        void assign<T>(out T discarded, in T value, inout bit<8> calls) {
            discarded = value;
            calls += 1;
        }
        control C() {
            bit<8> calls = 0;
            apply { assign(_, 8w7, calls); }
        }
    )");
    EXPECT_EQ(value("calls"_cs), 1U);
}

TEST_F(TypeInferenceTest, SavedArgumentsSurviveLaterSideEffectsAndNamedArgumentOrder) {
    evaluate(R"(
        T encode<T>(in T first, in T second) { return (first << 4) + second; }
        bit<8> next(inout bit<8> count) { bit<8> old = count; count += 1; return old; }
        control C() {
            bit<8> count = 1;
            bit<8> saved = encode(count, next(count));
            bit<8> named = encode(second = next(count), first = next(count));
            apply {}
        }
    )");
    EXPECT_EQ(value("saved"_cs), 17U);
    EXPECT_EQ(value("named"_cs), 50U);
    EXPECT_EQ(value("count"_cs), 4U);
}

TEST_F(TypeInferenceTest, ConstructorsSpecializeTheirBodiesWithoutLeakingBindings) {
    evaluate(R"(
        control Box<T>(out T result)(T initial) {
            T saved = initial;
            apply { result = saved; }
        }
        control C() {
            Box(8w7) small;
            Box(16w300) large;
            bit<8> x;
            bit<16> y;
            apply { small.apply(x); large.apply(y); }
        }
    )");
    EXPECT_EQ(value("x"_cs), 7U);
    EXPECT_EQ(value("y"_cs), 300U);
    EXPECT_EQ(state.check_for_type("T"_cs), nullptr);
}

TEST_F(TypeInferenceTest, NestedArrayElementTypesAreInferred) {
    evaluate(R"(
        T first<T>(in T[2] values) { return values[0]; }
        control C() {
            bit<8>[2] values = { 7, 9 };
            bit<8> result = first(values);
            apply {}
        }
    )");
    EXPECT_EQ(value("result"_cs), 7U);
}

TEST_F(TypeInferenceTest, ExternArrayConstructorsSpecializeElementsAndKeepResultsUninterpreted) {
    evaluate(R"(
        extern Cell<T> { Cell(T initial); T get(); }
        typedef Cell<bit<16>> WideCell;
        control C() {
            Cell(8w7) inferred[2];
            Cell<bit<16>>(16w300) explicitCells[1];
            WideCell(16w400) aliased[1];
            bit<8> small;
            bit<16> wide; bit<16> aliasValue;
            apply {
                small = inferred[1].get();
                wide = explicitCells[0].get();
                aliasValue = aliased[0].get();
            }
        }
    )");
    for (const auto name : {"small"_cs, "wide"_cs, "aliasValue"_cs}) {
        const auto result = *state.get_var<NumericVal>(name)->get_val();
        EXPECT_EQ(result.get_sort().bv_size(), name == "small" ? 8U : 16U);
        z3::solver solver(ctx);
        solver.add(result == 0);
        EXPECT_EQ(solver.check(), z3::sat);
        solver.reset();
        solver.add(result == 1);
        EXPECT_EQ(solver.check(), z3::sat);
    }
    EXPECT_EQ(state.check_for_type("T"_cs), nullptr);
}

TEST_F(TypeInferenceTest, SymbolicExternArrayReceiversEvaluateArgumentsOnceAndCopyOut) {
    evaluate(R"(
        extern Cell<T> {
            Cell(T initial);
            T read(out T result, inout T changed, in bit<8> marker);
        }
        bit<1> selectIndex(in bit<1> index, inout bit<8> calls) {
            calls += 1;
            return index;
        }
        bit<8> argument(inout bit<8> calls) { calls += 1; return calls; }
        control C() {
            Cell(16w7) cells[2];
            bit<1> index;
            bit<8> calls = 0;
            bit<16> result = 42; bit<16> changed = 43; bit<16> returned;
            apply {
                returned = cells[selectIndex(index, calls)].read(result, changed, argument(calls));
            }
        }
    )");
    EXPECT_EQ(value("calls"_cs), 2U);
    for (const auto name : {"result"_cs, "changed"_cs, "returned"_cs}) {
        const auto result = *state.get_var<NumericVal>(name)->get_val();
        EXPECT_EQ(result.get_sort().bv_size(), 16U);
        z3::solver solver(ctx);
        solver.add(result == 0);
        EXPECT_EQ(solver.check(), z3::sat);
        solver.reset();
        solver.add(result == 1);
        EXPECT_EQ(solver.check(), z3::sat);
    }
}

TEST_F(TypeInferenceTest, UnresolvedOutputCannotUseAnUnrelatedOuterTypeBinding) {
    evaluate(R"(
        typedef bit<16> T;
        void output<T>(out T value) {}
        control C() { apply {} }
    )");
    const IR::MethodCallExpression call(new IR::PathExpression("output"_cs),
                                        {new IR::DefaultExpression()});
    EXPECT_THROW(visitor.visit(&call), UnsupportedFeatureError);
    EXPECT_EQ(state.get_type("T"_cs)->checkedTo<IR::Type_Bits>()->size, 16);
}

TEST_F(TypeInferenceTest, InferredTuplesKeepCallerTypesWithTheSameNameAsATypeParameter) {
    evaluate(R"(
        struct T { bit<8> field; }
        T identity<T>(in T value) { T local = value; return local; }
        control C() {
            tuple<T, T> input = { { 7 }, { 9 } };
            tuple<T, T> result = identity(input);
            bit<8> first = result[0].field;
            apply {}
        }
    )");
    EXPECT_EQ(value("first"_cs), 7U);
    EXPECT_TRUE(state.get_type("T"_cs)->is<IR::Type_Struct>());
}

TEST_F(TypeInferenceTest, ArchitectureDefaultsConsumeTypesInferredFromSuppliedBlocks) {
    const auto program = parseP4String(R"(
        control Stage<T>(out T result);
        control Empty<T>(out T result) { apply { result = (T) 0; } }
        control Used(out bit<8> result) { apply { result = 7; } }
        package Top<T>(Stage<T> first, Stage<T> second = Empty<T>());
        Top(Used()) main;
    )");
    ASSERT_NE(program, nullptr);
    visitor.visit(program);
    const auto summary = gen_state_from_instance(&visitor, get_main_decl(&state));
    ASSERT_EQ(summary.size(), 2U);
    ASSERT_EQ(summary.at("first"_cs).first.at(0).first, "result"_cs);
    ASSERT_EQ(summary.at("second"_cs).first.at(0).first, "result"_cs);
    EXPECT_EQ(summary.at("first"_cs).first.at(0).second.simplify().get_numeral_uint(), 7U);
    EXPECT_EQ(summary.at("second"_cs).first.at(0).second.simplify().get_numeral_uint(), 0U);
}

}  // namespace
}  // namespace P4::ToZ3
