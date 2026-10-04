#include <gtest/gtest.h>

#include "toz3/common/exceptions.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

class LoopTest : public ::testing::Test {
 protected:
    z3::context ctx;
    P4State state{&ctx};
    Z3Visitor visitor{&state};
    LoopTest() { state.push_scope(); }
    const IR::PathExpression *path(cstring name) { return new IR::PathExpression(name); }
    const IR::Statement *increment(cstring name) {
        return new IR::AssignmentStatement(path(name),
                                           new IR::Add(path(name), new IR::Constant(1)));
    }
    const IR::ForStatement *loop(cstring index, const IR::Expression *condition,
                                 const IR::Statement *body, unsigned start = 0,
                                 unsigned width = 4) {
        return new IR::ForStatement(
            {new IR::Declaration_Variable(IR::ID(index), IR::Type_Bits::get(width),
                                          new IR::Constant(start))},
            condition, {increment(index)}, body);
    }
    void number(cstring name, unsigned value, unsigned width = 16) {
        const auto *type = IR::Type_Bits::get(width);
        state.declare_var(name, new Z3Bitvector(&state, type, ctx.bv_val(value, width)), type);
    }
    z3::expr value(cstring name) { return *state.get_var(name)->to<NumericVal>()->get_val(); }
    void equivalent(const z3::expr &left, const z3::expr &right) {
        z3::solver solver(ctx);
        solver.add(left != right);
        EXPECT_EQ(solver.check(), z3::unsat);
    }
};

TEST_F(LoopTest, SymbolicBreakAndContinueSkipRemainingStatements) {
    number("sum"_cs, 0);
    auto stop = ctx.bool_const("stop");
    state.declare_var("stop"_cs, new Z3Bitvector(&state, &BOOL_TYPE, stop), &BOOL_TYPE);
    auto *body = new IR::BlockStatement(
        {new IR::IfStatement(new IR::Equ(path("i"_cs), new IR::Constant(1)),
                             new IR::ContinueStatement(), nullptr),
         new IR::IfStatement(
             new IR::LAnd(path("stop"_cs), new IR::Equ(path("i"_cs), new IR::Constant(2))),
             new IR::BreakStatement(), nullptr),
         increment("sum"_cs)});
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(4)), body));
    equivalent(value("sum"_cs), z3::ite(stop, ctx.bv_val(1, 16), ctx.bv_val(3, 16)));
}

TEST_F(LoopTest, NestedSymbolicLoopsHaveExactFiniteResults) {
    const auto *type = IR::Type_Bits::get(2);
    auto bound = ctx.bv_const("bound", 2);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    auto *inner =
        loop("j"_cs, new IR::Lss(path("j"_cs), path("bound"_cs)), increment("sum"_cs), 0, 2);
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), inner, 0, 2));
    auto count = z3::zext(bound, 14);
    equivalent(value("sum"_cs), count * count);
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, AdditiveSummaryPreservesModularOverflow) {
    const auto *type = IR::Type_Bits::get(8);
    auto bound = ctx.bv_const("bound", 8);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 5, 3);
    auto *body = new IR::AssignmentStatement(path("sum"_cs),
                                             new IR::Add(path("sum"_cs), new IR::Constant(7)));
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), body, 0, 8));
    equivalent(value("sum"_cs), ctx.bv_val(5, 3) + bound.extract(2, 0) * ctx.bv_val(7, 3));
}

TEST_F(LoopTest, CounterDependentBoundIsEvaluatedEveryIteration) {
    number("sum"_cs, 0);
    auto *condition = new IR::Lss(path("i"_cs), new IR::Add(path("i"_cs), new IR::Constant(1)));
    visitor.visit(loop("i"_cs, condition, increment("sum"_cs), 0, 3));
    equivalent(value("sum"_cs), ctx.bv_val(7, 16));
}

TEST_F(LoopTest, CounterWrapIsReportedAsNontermination) {
    const auto *type = IR::Type_Bits::get(3);
    auto bound = ctx.bv_const("bound", 3);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    auto *body = new IR::AssignmentStatement(
        path("sum"_cs),
        new IR::Add(path("sum"_cs), new IR::Cast(IR::Type_Bits::get(16), path("i"_cs))));
    visitor.visit(loop("i"_cs, new IR::Leq(path("i"_cs), path("bound"_cs)), body, 1, 3));
    equivalent(state.get_termination_condition(), bound != ctx.bv_val(7, 3));
    auto count = z3::zext(bound, 13);
    z3::solver solver(ctx);
    solver.add(state.get_termination_condition());
    solver.add(value("sum"_cs) != z3::udiv(count * (count + 1), ctx.bv_val(2, 16)));
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST_F(LoopTest, CounterWrapDoesNotHideALaterBreak) {
    number("sum"_cs, 0);
    auto *body = new IR::BlockStatement(
        {increment("sum"_cs), new IR::IfStatement(new IR::Equ(path("sum"_cs), new IR::Constant(6)),
                                                  new IR::BreakStatement(), nullptr)});
    visitor.visit(loop("i"_cs, new IR::BoolLiteral(true), body, 0, 2));
    equivalent(value("sum"_cs), ctx.bv_val(6, 16));
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, RepeatedCounterWithChangingUpdateInputsCanTerminate) {
    number("sum"_cs, 1, 2);
    auto *statement =
        loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(3)), increment("sum"_cs), 0, 2);
    auto *changing = statement->clone();
    changing->updates = {
        new IR::AssignmentStatement(path("i"_cs), new IR::Add(path("i"_cs), path("sum"_cs)))};
    visitor.visit(static_cast<const IR::ForStatement *>(changing));
    equivalent(value("sum"_cs), ctx.bv_val(3, 2));
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, MultipleUpdatesArePreservedOnEachSymbolicFinishingPath) {
    const auto *type = IR::Type_Bits::get(2);
    auto bound = ctx.bv_const("bound", 2);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    auto *statement =
        loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), new IR::EmptyStatement(), 0, 2);
    auto *multiple = statement->clone();
    multiple->updates.push_back(increment("sum"_cs));
    visitor.visit(static_cast<const IR::ForStatement *>(multiple));
    equivalent(value("sum"_cs), z3::zext(bound, 14));
}

TEST_F(LoopTest, ReturnsLeaveTheLoopAndPreserveTheOuterScope) {
    number("i"_cs, 7);
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(4)),
                       new IR::ReturnStatement(new IR::Constant(42))));
    EXPECT_TRUE(state.has_returned());
    equivalent(value("i"_cs), ctx.bv_val(7, 16));
    const auto results = state.get_return_exprs();
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results.front().second->to<NumericVal>()->get_val()->get_numeral_uint(), 42U);
}

TEST_F(LoopTest, SignedRangesIncludeNegativeValues) {
    number("sum"_cs, 0);
    const auto *type = IR::Type_Bits::get(4, true);
    IR::ForInStatement statement(
        new IR::Declaration_Variable(IR::ID("i"_cs), type),
        new IR::Range(new IR::Constant(type, -2), new IR::Constant(type, 2)), increment("sum"_cs));
    visitor.visit(&statement);
    equivalent(value("sum"_cs), ctx.bv_val(5, 16));
}

TEST_F(LoopTest, ForInBreakPreservesAnExistingCounter) {
    number("sum"_cs, 0);
    number("i"_cs, 0);
    auto stop = ctx.bool_const("stop");
    state.declare_var("stop"_cs, new Z3Bitvector(&state, &BOOL_TYPE, stop), &BOOL_TYPE);
    auto *body = new IR::BlockStatement(
        {new IR::IfStatement(
             new IR::LAnd(path("stop"_cs), new IR::Equ(path("i"_cs), new IR::Constant(1))),
             new IR::BreakStatement(), nullptr),
         increment("sum"_cs)});
    IR::ForInStatement statement(
        new IR::Declaration_Variable(IR::ID("i"_cs), IR::Type_Bits::get(16)),
        new IR::Range(new IR::Constant(0), new IR::Constant(3)), body);
    statement.decl = nullptr;  // MoveDeclarations can move the declaration outside the loop.
    visitor.visit(&statement);
    equivalent(value("sum"_cs), z3::ite(stop, ctx.bv_val(1, 16), ctx.bv_val(4, 16)));
    equivalent(value("i"_cs), z3::ite(stop, ctx.bv_val(1, 16), ctx.bv_val(3, 16)));
}

}  // namespace
}  // namespace P4::ToZ3
