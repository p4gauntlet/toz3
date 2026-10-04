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

class TrackedValue : public Z3Bitvector {
    bool *destroyed;

 public:
    TrackedValue(P4State *state, const z3::expr &value, bool *destroyed)
        : Z3Bitvector(state, &BOOL_TYPE, value), destroyed(destroyed) {}
    ~TrackedValue() override { *destroyed = true; }
};

TEST(VisitorLifetime, StateReleasesValuesWhileExportedExpressionsRemainAlive) {
    z3::context ctx;
    bool destroyed = false;
    z3::expr summary(ctx);
    {
        P4State state(&ctx);
        auto *value =
            allocate_instance<TrackedValue>(&state, &state, ctx.bool_const("input"), &destroyed);
        state.push_scope();
        state.declare_var("value"_cs, value, &BOOL_TYPE);
        auto snapshot = state.clone_vars();
        summary = *snapshot.at("value"_cs).first->to<Z3Bitvector>()->get_val();
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(z3::eq(summary, ctx.bool_const("input")));
}

}  // namespace
}  // namespace P4::ToZ3
