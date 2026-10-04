#include <gtest/gtest.h>

#include "frontends/common/parseInput.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {
class ReceiverTest : public ::testing::Test {
 protected:
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};
    z3::context ctx;
    P4State state{&ctx};
    Z3Visitor visitor{&state};
    void evaluate(const char *source) {
        const auto *program = parseP4String(source);
        ASSERT_NE(program, nullptr);
        visitor.visit(program);
        const auto *control = state.get_type("C"_cs)->to<IR::P4Control>();
        ASSERT_NE(control, nullptr);
        for (const auto *local : control->controlLocals) visitor.visit(local);
        visitor.visit(control->body);
    }
    unsigned value(cstring name) {
        return state.get_var(name)->to<NumericVal>()->get_val()->simplify().get_numeral_uint();
    }
};

TEST_F(ReceiverTest, ReturnedHeaderReceiverEvaluatesFunctionExactlyOnce) {
    evaluate(R"(
        header H { bit<8> x; }
        H f(inout bit<8> count) {
            count += 1;
            H h;
            h.setValid();
            return h;
        }
        control C() { bit<8> calls = 0; bool valid = f(calls).isValid(); apply {} }
    )");
    EXPECT_EQ(value("calls"_cs), 1U);
    EXPECT_TRUE(state.get_var("valid"_cs)->to<NumericVal>()->get_val()->simplify().is_true());
}

TEST_F(ReceiverTest, CompoundAssignmentSavesIndexAndOldValueBeforeRightSide) {
    evaluate(R"(
        bit<8> next(inout bit<8> i) { bit<8> old = i; i += 1; return old; }
        bit<8> rhs(inout bit<8> value) { value = 99; return 5; }
        void run(inout bit<8>[2] v, inout bit<8> i) {
            v[next(i)] += rhs(v[0]);
        }
        control C() { bit<8> index = 0; bit<8>[2] values = { 3, 7 }; apply {} }
    )");
    const IR::MethodCallExpression call(
        new IR::PathExpression("run"_cs),
        new IR::Vector<IR::Argument>{new IR::Argument(new IR::PathExpression("values"_cs)),
                                     new IR::Argument(new IR::PathExpression("index"_cs))});
    visitor.visit(&call);
    EXPECT_EQ(value("index"_cs), 1U);
    const auto *array = state.get_var<StackInstance>("values"_cs);
    EXPECT_EQ(array->get_member("0"_cs)->to<NumericVal>()->get_val()->simplify().get_numeral_uint(),
              8U);
    EXPECT_EQ(array->get_member("1"_cs)->to<NumericVal>()->get_val()->simplify().get_numeral_uint(),
              7U);
}

TEST_F(ReceiverTest, NamedArgumentsEvaluateInCallSiteOrder) {
    evaluate(R"(
        bit<8> next(inout bit<8> i) { bit<8> old = i; i += 1; return old; }
        void capture(inout bit<8> z, in bit<8> first, in bit<8> second) {
            z = (first << 4) + second;
        }
        void run(inout bit<8> z, inout bit<8> i) {
            capture(z = z, second = next(i), first = next(i));
        }
        control C() { bit<8> calls = 0; bit<8> result = 0; apply {} }
    )");
    const IR::MethodCallExpression call(
        new IR::PathExpression("run"_cs),
        {new IR::PathExpression("result"_cs), new IR::PathExpression("calls"_cs)});
    visitor.visit(&call);
    EXPECT_EQ(value("calls"_cs), 2U);
    EXPECT_EQ(value("result"_cs), 16U);
}

TEST_F(ReceiverTest, DefaultHeaderDiffersFromDefaultFieldInitializer) {
    evaluate(R"(
        header H { bit<8> x; }
        enum E { A, B }
        enum bit<8> Z { A = 2, B = 3 }
        control C() {
        H invalid = ...;
        H valid = { ... };
        H partial = { 7, ... };
        H[2] stack = ...;
        E e = ...;
        Z z = ...; apply {} }
    )");
    EXPECT_TRUE(state.get_var<HeaderInstance>("invalid"_cs)->get_valid()->is_false());
    const auto *valid = state.get_var<HeaderInstance>("valid"_cs);
    EXPECT_TRUE(valid->get_valid()->is_true());
    EXPECT_EQ(valid->get_member("x"_cs)->to<NumericVal>()->get_val()->get_numeral_uint(), 0U);
    EXPECT_EQ(state.get_var<HeaderInstance>("partial"_cs)
                  ->get_member("x"_cs)
                  ->to<NumericVal>()
                  ->get_val()
                  ->get_numeral_uint(),
              7U);
    const auto *stack = state.get_var<StackInstance>("stack"_cs);
    EXPECT_TRUE(stack->get_member("0"_cs)->to<HeaderInstance>()->get_valid()->is_false());
    EXPECT_TRUE(stack->get_member("1"_cs)->to<HeaderInstance>()->get_valid()->is_false());
    EXPECT_EQ(state.get_var<EnumInstance>("e"_cs)->get_val()->get_numeral_uint(), 0U);
    EXPECT_EQ(state.get_var<SerEnumInstance>("z"_cs)->get_val()->get_numeral_uint(), 0U);
}

TEST_F(ReceiverTest, SizeMethodDoesNotEvaluateItsReceiver) {
    evaluate(R"(
        header H { bit<7> x; varbit<9> y; }
        H f(inout bit<8> count) { count += 1; H h; return h; }
        control C() { bit<8> calls = 0;
        const int minimum = f(calls).minSizeInBits();
        const int maximum = f(calls).maxSizeInBytes();
        H[0] empty;
        const int element = empty[0].maxSizeInBits(); apply {} }
    )");
    EXPECT_EQ(value("calls"_cs), 0U);
    EXPECT_EQ(value("minimum"_cs), 7U);
    EXPECT_EQ(value("maximum"_cs), 2U);
    EXPECT_EQ(value("element"_cs), 16U);
}

TEST_F(ReceiverTest, ZeroWidthValuesRemainZeroAcrossArithmeticAndBinding) {
    evaluate(R"(
        enum bit<0> E { Z = 0 }
        header H { bit<0> empty; bit<8> field; }
        control C() {
        bit<0> a = 7;
        bit<0> b = a + (bit<0>) 255;
        bit<8> c = (bit<8>) b;
        H h; apply {} }
    )");
    EXPECT_EQ(value("a"_cs), 0U);
    state.get_var("a"_cs)->set_undefined();
    EXPECT_EQ(value("a"_cs), 0U);
    EXPECT_EQ(value("b"_cs), 0U);
    EXPECT_EQ(value("c"_cs), 0U);
    const auto *enumeration = state.get_var<SerEnumInstance>("E"_cs);
    EXPECT_EQ(enumeration->get_val()->get_numeral_uint(), 0U);
    auto *header = state.get_var<HeaderInstance>("h"_cs)->copy();
    header->bind(nullptr, 0);
    EXPECT_EQ(header->get_width(), 8U);
    EXPECT_EQ(header->get_member("empty"_cs)->to<NumericVal>()->get_val()->get_numeral_uint(), 0U);
}

TEST_F(ReceiverTest, MatchKindsFromSeparateDeclarationsHaveDistinctValues) {
    evaluate(R"(
        match_kind { exact, ternary }
        match_kind { range }
        control C() { apply {} }
    )");
    const auto *exact = state.get_var<NumericVal>("exact"_cs);
    const auto *ternary = state.get_var<NumericVal>("ternary"_cs);
    const auto *range = state.get_var<NumericVal>("range"_cs);
    EXPECT_TRUE((*exact->get_val() != *ternary->get_val()).simplify().is_true());
    EXPECT_TRUE((*exact->get_val() != *range->get_val()).simplify().is_true());
    EXPECT_TRUE((*ternary->get_val() != *range->get_val()).simplify().is_true());
}

}  // namespace
}  // namespace P4::ToZ3
