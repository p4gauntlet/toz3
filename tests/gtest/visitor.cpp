#include <gtest/gtest.h>

#include <memory>

#include "absl/base/attributes.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

ABSL_ATTRIBUTE_NOINLINE std::unique_ptr<Z3Visitor> makeVisitor(P4State *state) {
    return std::make_unique<Z3Visitor>(state);
}

TEST(VisitorLifetime, StandaloneVisitorCanEvaluateMultipleRoots) {
    z3::context ctx;
    P4State state(&ctx);
    auto visitor = makeVisitor(&state);
    for (unsigned idx = 0; idx < 128; ++idx) {
        IR::Constant literal(IR::Type_Bits::get(8), idx);
        visitor->visit(&literal);
        EXPECT_EQ(state.get_expr_result<NumericVal>()->get_val()->get_numeral_uint(), idx);
    }
}

}  // namespace
}  // namespace P4::ToZ3
