#include <gtest/gtest.h>

#include "frontends/common/parser_options.h"
#include "toz3/common/exceptions.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

constexpr int VALUE_WIDTH = 16;
constexpr int LOOP_COUNTER_WIDTH = 8;

struct LoopTestState {
    z3::context ctx;
    P4State state{&ctx};
    Z3Visitor visitor{&state};
};

class LoopTest : public ::testing::Test, protected LoopTestState {
 protected:
    LoopTest() { state.push_scope(); }
    static const IR::PathExpression *path(cstring name) { return new IR::PathExpression(name); }
    static const IR::Statement *increment(cstring name) {
        return new IR::AssignmentStatement(path(name),
                                           new IR::Add(path(name), new IR::Constant(1)));
    }
    static const IR::ForStatement *loop(cstring index, const IR::Expression *condition,
                                        const IR::Statement *body, unsigned start = 0,
                                        int width = 4) {
        return new IR::ForStatement(
            {new IR::Declaration_Variable(IR::ID(index), IR::Type_Bits::get(width),
                                          new IR::Constant(start))},
            condition, {increment(index)}, body);
    }
    void number(cstring name, unsigned value, int width = VALUE_WIDTH) {
        const auto type = IR::Type_Bits::get(width);
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
    equivalent(value("sum"_cs),
               z3::ite(stop, ctx.bv_val(1, VALUE_WIDTH), ctx.bv_val(3, VALUE_WIDTH)));
}

TEST_F(LoopTest, NestedSymbolicLoopsHaveExactFiniteResults) {
    const auto type = IR::Type_Bits::get(2);
    auto bound = ctx.bv_const("bound", 2);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    const auto *inner =
        loop("j"_cs, new IR::Lss(path("j"_cs), path("bound"_cs)), increment("sum"_cs), 0, 2);
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), inner, 0, 2));
    auto count = z3::zext(bound, VALUE_WIDTH - 2);
    equivalent(value("sum"_cs), count * count);
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, AdditiveSummaryPreservesModularOverflow) {
    constexpr unsigned START = 5;
    constexpr unsigned STEP = 7;
    const auto type = IR::Type_Bits::get(LOOP_COUNTER_WIDTH);
    auto bound = ctx.bv_const("bound", LOOP_COUNTER_WIDTH);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, START, 3);
    auto *body = new IR::AssignmentStatement(path("sum"_cs),
                                             new IR::Add(path("sum"_cs), new IR::Constant(STEP)));
    visitor.visit(
        loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), body, 0, LOOP_COUNTER_WIDTH));
    equivalent(value("sum"_cs), ctx.bv_val(START, 3) + bound.extract(2, 0) * ctx.bv_val(STEP, 3));
}

TEST_F(LoopTest, CounterDependentBoundIsEvaluatedEveryIteration) {
    number("sum"_cs, 0);
    auto *condition = new IR::Lss(path("i"_cs), new IR::Add(path("i"_cs), new IR::Constant(1)));
    visitor.visit(loop("i"_cs, condition, increment("sum"_cs), 0, 3));
    equivalent(value("sum"_cs), ctx.bv_val((1U << 3U) - 1, VALUE_WIDTH));
}

TEST_F(LoopTest, CounterWrapIsReportedAsNontermination) {
    const auto type = IR::Type_Bits::get(3);
    auto bound = ctx.bv_const("bound", 3);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    auto *body = new IR::AssignmentStatement(
        path("sum"_cs),
        new IR::Add(path("sum"_cs), new IR::Cast(IR::Type_Bits::get(VALUE_WIDTH), path("i"_cs))));
    visitor.visit(loop("i"_cs, new IR::Leq(path("i"_cs), path("bound"_cs)), body, 1, 3));
    equivalent(state.get_termination_condition(), bound != ctx.bv_val(-1, 3));
    auto count = z3::zext(bound, VALUE_WIDTH - 3);
    z3::solver solver(ctx);
    solver.add(state.get_termination_condition());
    solver.add(value("sum"_cs) != z3::udiv(count * (count + 1), ctx.bv_val(2, VALUE_WIDTH)));
    EXPECT_EQ(solver.check(), z3::unsat);
}

TEST_F(LoopTest, CounterWrapDoesNotHideALaterBreak) {
    constexpr unsigned BREAK_COUNT = 6;
    number("sum"_cs, 0);
    auto *body = new IR::BlockStatement(
        {increment("sum"_cs),
         new IR::IfStatement(new IR::Equ(path("sum"_cs), new IR::Constant(BREAK_COUNT)),
                             new IR::BreakStatement(), nullptr)});
    visitor.visit(loop("i"_cs, new IR::BoolLiteral(true), body, 0, 2));
    equivalent(value("sum"_cs), ctx.bv_val(BREAK_COUNT, VALUE_WIDTH));
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, RepeatedCounterWithChangingUpdateInputsCanTerminate) {
    number("sum"_cs, 1, 2);
    const auto *statement =
        loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(3)), increment("sum"_cs), 0, 2);
    auto *changing = statement->clone();
    changing->updates = {
        new IR::AssignmentStatement(path("i"_cs), new IR::Add(path("i"_cs), path("sum"_cs)))};
    visitor.visit(static_cast<const IR::ForStatement *>(changing));
    equivalent(value("sum"_cs), ctx.bv_val(3, 2));
    EXPECT_TRUE(state.get_termination_condition().simplify().is_true());
}

TEST_F(LoopTest, MultipleUpdatesArePreservedOnEachSymbolicFinishingPath) {
    const auto type = IR::Type_Bits::get(2);
    auto bound = ctx.bv_const("bound", 2);
    state.declare_var("bound"_cs, new Z3Bitvector(&state, type, bound), type);
    number("sum"_cs, 0);
    const auto *statement =
        loop("i"_cs, new IR::Lss(path("i"_cs), path("bound"_cs)), new IR::EmptyStatement(), 0, 2);
    auto *multiple = statement->clone();
    multiple->updates.push_back(increment("sum"_cs));
    visitor.visit(static_cast<const IR::ForStatement *>(multiple));
    equivalent(value("sum"_cs), z3::zext(bound, VALUE_WIDTH - 2));
}

TEST_F(LoopTest, ReturnsLeaveTheLoopAndPreserveTheOuterScope) {
    constexpr unsigned OUTER_COUNTER = 7;
    constexpr unsigned RETURN_VALUE = 42;
    number("i"_cs, OUTER_COUNTER);
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(4)),
                       new IR::ReturnStatement(new IR::Constant(RETURN_VALUE))));
    EXPECT_TRUE(state.has_returned());
    equivalent(value("i"_cs), ctx.bv_val(OUTER_COUNTER, VALUE_WIDTH));
    const auto results = state.get_return_exprs();
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results.front().second->to<NumericVal>()->get_val()->get_numeral_uint(), 42U);
}

TEST_F(LoopTest, BreakInSwitchExitsTheLoopWithoutExecutingItsUpdates) {
    number("sum"_cs, 0);
    const auto *statement = new IR::SwitchStatement(
        path("i"_cs),
        {new IR::SwitchCase(new IR::Constant(0), new IR::BlockStatement({increment("sum"_cs)})),
         new IR::SwitchCase(new IR::Constant(1),
                            new IR::BlockStatement({new IR::BreakStatement()})),
         new IR::SwitchCase(new IR::DefaultExpression(),
                            new IR::BlockStatement({increment("sum"_cs)}))});
    visitor.visit(loop("i"_cs, new IR::Lss(path("i"_cs), new IR::Constant(4)), statement));
    equivalent(value("sum"_cs), ctx.bv_val(1, VALUE_WIDTH));
}

TEST_F(LoopTest, ImmutableSwitchUsesEntriesAndDefaultForEveryKey) {
    AutoCompileContext context(new P4CContextWithOptions<ParserOptions>);
    const auto key = ctx.bv_const("key", 2);
    const auto bits = IR::Type_Bits::get(2);
    state.declare_var("key"_cs, new Z3Bitvector(&state, bits, key), bits);
    number("sum"_cs, 0);
    const auto *entryAction = new IR::MethodCallExpression(path("entry"_cs));
    const auto *defaultAction = new IR::MethodCallExpression(path("fallback"_cs));
    TableProperties properties;
    properties.immutable = true;
    properties.keys = {new IR::KeyElement(path("key"_cs), path("exact"_cs))};
    properties.entries = {{new IR::ListExpression({new IR::Constant(bits, 1)}), entryAction}};
    properties.default_action = defaultAction;
    const auto *actions = new IR::ActionList(
        {new IR::ActionListElement(entryAction), new IR::ActionListElement(defaultAction)});
    const auto *declaration = new IR::P4Table(
        IR::ID("t"_cs), new IR::TableProperties({new IR::Property("actions", actions, false)}));
    auto *table = new P4TableInstance(&state, declaration, ctx.bool_val(true), properties);
    for (const auto name : {"entry"_cs, "fallback"_cs}) {
        const auto *action =
            new IR::P4Action(IR::ID(name), new IR::ParameterList, new IR::BlockStatement);
        visitor.visit(action);
    }
    const IR::Vector<IR::Argument> arguments;
    table->apply(&visitor, &arguments);
    const auto *result = state.get_expr_result<P4TableInstance>();
    equivalent(result->hit, key == ctx.bv_val(1, 2));
    state.declare_var("t"_cs, result->copy(), &BOOL_TYPE);
    // action_run describes the lookup, even if its key variable later changes.
    state.update_var("key"_cs, new Z3Bitvector(&state, bits, ctx.bv_val(0, 2)));
    const auto *statement = new IR::SwitchStatement(
        path("t"_cs),
        {new IR::SwitchCase(path("entry"_cs), new IR::BlockStatement({increment("sum"_cs)})),
         new IR::SwitchCase(path("fallback"_cs),
                            new IR::BlockStatement({increment("sum"_cs), increment("sum"_cs)}))});
    visitor.visit(statement);
    equivalent(value("sum"_cs), z3::ite(key == ctx.bv_val(1, 2), ctx.bv_val(1, VALUE_WIDTH),
                                        ctx.bv_val(2, VALUE_WIDTH)));
}

TEST_F(LoopTest, SwitchFallThroughPreservesTheImplicitDefaultPath) {
    const auto bits = IR::Type_Bits::get(2);
    const auto key = ctx.bv_const("key", 2);
    state.declare_var("key"_cs, state.gen_instance("key"_cs, bits), bits);
    number("sum"_cs, 0);
    const IR::SwitchStatement statement(
        path("key"_cs),
        {new IR::SwitchCase(new IR::Constant(0), nullptr),
         new IR::SwitchCase(new IR::Constant(1), new IR::BlockStatement({increment("sum"_cs)}))});
    visitor.visit(&statement);
    equivalent(value("sum"_cs), z3::ite(z3::ule(key, ctx.bv_val(1, 2)), ctx.bv_val(1, VALUE_WIDTH),
                                        ctx.bv_val(0, VALUE_WIDTH)));
}

TEST_F(LoopTest, SignedRangesIncludeNegativeValues) {
    number("sum"_cs, 0);
    const auto type = IR::Type_Bits::get(4, true);
    IR::ForInStatement statement(
        new IR::Declaration_Variable(IR::ID("i"_cs), type),
        new IR::Range(new IR::Constant(type, -2), new IR::Constant(type, 2)), increment("sum"_cs));
    visitor.visit(&statement);
    constexpr int RANGE_SIZE = 2 - (-2) + 1;
    equivalent(value("sum"_cs), ctx.bv_val(RANGE_SIZE, VALUE_WIDTH));
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
        new IR::Declaration_Variable(IR::ID("i"_cs), IR::Type_Bits::get(VALUE_WIDTH)),
        new IR::Range(new IR::Constant(0), new IR::Constant(3)), body);
    statement.decl = nullptr;  // MoveDeclarations can move the declaration outside the loop.
    visitor.visit(&statement);
    equivalent(value("sum"_cs),
               z3::ite(stop, ctx.bv_val(1, VALUE_WIDTH), ctx.bv_val(4, VALUE_WIDTH)));
    equivalent(value("i"_cs),
               z3::ite(stop, ctx.bv_val(1, VALUE_WIDTH), ctx.bv_val(3, VALUE_WIDTH)));
}

TEST_F(LoopTest, SymbolicRangesIncludeBothBoundsAndPreserveSnapshot) {
    number("sum"_cs, 0);
    const auto bits = IR::Type_Bits::get(2);
    const auto lower = ctx.bv_const("lower", 2);
    const auto upper = ctx.bv_const("upper", 2);
    state.declare_var("lower"_cs, new Z3Bitvector(&state, bits, lower), bits);
    state.declare_var("upper"_cs, new Z3Bitvector(&state, bits, upper), bits);
    const auto *body = new IR::BlockStatement(
        {increment("sum"_cs), new IR::AssignmentStatement(path("upper"_cs), new IR::Constant(0))});
    const IR::ForInStatement statement(new IR::Declaration_Variable(IR::ID("i"_cs), bits),
                                       new IR::Range(path("lower"_cs), path("upper"_cs)), body);
    visitor.visit(&statement);
    const auto count = z3::zext(upper, 14) - z3::zext(lower, 14) + ctx.bv_val(1, 16);
    equivalent(value("sum"_cs), z3::ite(z3::ule(lower, upper), count, ctx.bv_val(0, VALUE_WIDTH)));
}

TEST_F(LoopTest, InvariantConditionReportsExactlyTheNonterminatingPaths) {
    number("sum"_cs, 0);
    auto infinite = ctx.bool_const("infinite");
    state.declare_var("infinite"_cs, new Z3Bitvector(&state, &BOOL_TYPE, infinite), &BOOL_TYPE);
    const IR::ForStatement statement({}, path("infinite"_cs), {}, increment("sum"_cs));
    visitor.visit(&statement);
    equivalent(state.get_termination_condition(), !infinite);
}

}  // namespace
}  // namespace P4::ToZ3
