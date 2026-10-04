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

}  // namespace
}  // namespace P4::ToZ3
