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

}  // namespace
}  // namespace P4::ToZ3
