#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "frontends/common/parseInput.h"
#include "toz3/common/create_z3.h"
#include "toz3/common/util.h"
#include "toz3/compare/compare.h"

namespace P4::ToZ3 {
namespace {

// Expectations follow P4-16 sec-expr-hu, including setInvalid on an inactive member.
class HeaderUnionTest : public ::testing::Test {
 protected:
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};
    z3::context ctx;
    P4State state{&ctx};
    Z3Visitor visitor{&state};
    HeaderUnionTest() { state.push_scope(); }

    void evaluate(const char *source) {
        const auto *program = parseP4String(std::string(R"(
            header H { bit<8> x; }
            header_union U { H a; H b; }
        )") + source);
        ASSERT_NE(program, nullptr);
        visitor.visit(program);
        const auto *control = state.get_type("C"_cs)->checkedTo<IR::P4Control>();
        for (const auto *local : control->controlLocals) visitor.visit(local);
        visitor.visit(control->body);
    }
    void equivalent(const z3::expr &actual, const z3::expr &expected) {
        z3::solver solver(ctx);
        solver.add(state.get_input_constraints() && actual != expected);
        EXPECT_EQ(solver.check(), z3::unsat) << actual << " != " << expected;
    }
    void boolean(cstring name, bool expected) {
        equivalent(*state.get_var<NumericVal>(name)->get_val(), ctx.bool_val(expected));
    }
    z3::expr valid(const P4Z3Instance *object, cstring member) {
        return *object->get_member(member)->checkedTo<HeaderInstance>()->get_valid();
    }
};

TEST_F(HeaderUnionTest, ValidityMethodsSelectAndClearTheEntireUnion) {
    evaluate(R"(
        control C() {
            U u;
            bool initial = u.isValid();
            bool selected; bool sibling; bool cleared;
            apply {
                u.a.setValid();
                u.b.setValid();
                selected = u.b.isValid();
                sibling = u.a.isValid();
                u.a.setInvalid();
                cleared = !u.isValid();
            }
        }
    )");
    boolean("initial"_cs, false);
    boolean("selected"_cs, true);
    boolean("sibling"_cs, false);
    boolean("cleared"_cs, true);
}

TEST_F(HeaderUnionTest, HeaderAndTupleAssignmentsSelectAMemberAndInvalidAssignmentClearsIt) {
    evaluate(R"(
        control C() {
            U u; H empty; H source = { 7 };
            bool selected; bool sibling; bool cleared; bit<8> value;
            apply {
                u.a = { 9 };
                u.b = source;
                selected = u.b.isValid(); sibling = u.a.isValid(); value = u.b.x;
                u.a = empty;
                cleared = !u.isValid();
            }
        }
    )");
    boolean("selected"_cs, true);
    boolean("sibling"_cs, false);
    boolean("cleared"_cs, true);
    equivalent(*state.get_var<NumericVal>("value"_cs)->get_val(), ctx.bv_val(7, 8));
}

TEST_F(HeaderUnionTest, CopiesHaveIndependentMembershipAndPreserveUnionState) {
    evaluate(R"(
        control C() {
            U source; U copy; U empty; H detached;
            bool same; bool original; bool replacement; bool cleared;
            apply {
                source.a = { 11 };
                copy = source;
                same = copy == source;
                detached = copy.a;
                detached.setInvalid();
                copy.b = { 22 };
                original = source.a.isValid() && !source.b.isValid();
                replacement = copy.b.isValid() && !copy.a.isValid();
                copy = empty;
                cleared = !copy.isValid();
            }
        }
    )");
    boolean("same"_cs, true);
    boolean("original"_cs, true);
    boolean("replacement"_cs, true);
    boolean("cleared"_cs, true);
}

TEST_F(HeaderUnionTest, ConditionalUpdatesMergeAllMemberValidityBits) {
    evaluate(R"(
        control C() {
            U u; bool choose;
            apply {
                u.a = { 1 };
                if (choose) { u.b = { 2 }; }
            }
        }
    )");
    const auto choose = *state.get_var<NumericVal>("choose"_cs)->get_val();
    const auto *u = state.get_var("u"_cs);
    equivalent(valid(u, "a"_cs), !choose);
    equivalent(valid(u, "b"_cs), choose);
}

TEST_F(HeaderUnionTest, SymbolicStackReceiversAndAssignmentsGuardSiblingEffects) {
    evaluate(R"(
        control C() {
            U[2] us; bit<1> index;
            apply {
                us[0].a = { 1 }; us[1].a = { 2 };
                us[index].b.setValid();
                us[index].b = { 3 };
            }
        }
    )");
    const auto index = *state.get_var<NumericVal>("index"_cs)->get_val();
    const auto *us = state.get_var("us"_cs);
    for (unsigned i = 0; i < 2; ++i) {
        const auto *u = us->get_member(cstring(std::to_string(i)));
        equivalent(valid(u, "a"_cs), index != ctx.bv_val(i, 1));
        equivalent(valid(u, "b"_cs), index == ctx.bv_val(i, 1));
    }
}

TEST_F(HeaderUnionTest, CopyOutToAUnionMemberUsesUnionAssignmentSemantics) {
    evaluate(R"(
        void fill(out H hdr) { hdr = { 42 }; }
        control C() {
            U u; bool selected;
            apply {
                u.a = { 1 };
                fill(u.b);
                selected = u.b.isValid() && !u.a.isValid();
            }
        }
    )");
    boolean("selected"_cs, true);
}

TEST_F(HeaderUnionTest, SymbolicValidityQueriesSelectTheReceiverResult) {
    evaluate(R"(
        control C() {
            U[2] us; bit<1> index; bool union_valid; bool member_valid;
            apply {
                us[0].a = { 1 };
                union_valid = us[index].isValid();
                member_valid = us[index].a.isValid();
            }
        }
    )");
    const auto selected = *state.get_var<NumericVal>("index"_cs)->get_val() == ctx.bv_val(0, 1);
    equivalent(*state.get_var<NumericVal>("union_valid"_cs)->get_val(), selected);
    equivalent(*state.get_var<NumericVal>("member_valid"_cs)->get_val(), selected);
}

TEST_F(HeaderUnionTest, InvalidMemberFieldWritesMayClobberSiblingDataButNotValidity) {
    evaluate(R"(
        struct Payload { bit<8> x; }
        header Nested { Payload p; }
        header_union N { Nested a; Nested b; }
        control C() {
            N u; bit<8> value; bool selected;
            apply {
                u.a.setValid(); u.a.p.x = 7;
                u.b.p.x = 2;
                value = u.a.p.x;
                selected = u.a.isValid() && !u.b.isValid();
            }
        }
    )");
    boolean("selected"_cs, true);
    EXPECT_TRUE(defined_when(*state.get_var<NumericVal>("value"_cs)->get_val()).is_false());
}

TEST_F(HeaderUnionTest, SummaryMasksInactiveMembersAndIncludesValidity) {
    evaluate(R"(
        control C() { U u; apply { u.a = { 7 }; } }
    )");
    const auto summary = state.get_var<StructBase>("u"_cs)->get_z3_vars("u"_cs);
    ASSERT_EQ(summary.size(), 4U);
    equivalent(summary[0].second, ctx.bool_val(true));
    equivalent(summary[1].second, ctx.bv_val(7, 8));
    equivalent(summary[2].second, ctx.bool_val(false));
    equivalent(summary[3].second, ctx.bv_const("invalid", 8));
}

TEST_F(HeaderUnionTest, SymbolicInputsAllowEveryLegalUnionStateAndNoOthers) {
    evaluate(R"(
        control C() { U u; apply {} }
    )");
    auto *u = state.get_var("u"_cs)->checkedTo<HeaderUnionInstance>();
    u->propagate_validity(nullptr);
    const auto a = valid(u, "a"_cs), b = valid(u, "b"_cs);
    for (bool av : {false, true}) {
        for (bool bv : {false, true}) {
            z3::solver solver(ctx);
            solver.add(state.get_input_constraints() && a == ctx.bool_val(av) &&
                       b == ctx.bool_val(bv));
            EXPECT_EQ(solver.check(), av && bv ? z3::unsat : z3::sat);
        }
    }
}

class HeaderUnionComparison : public ::testing::Test {
 protected:
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};
    std::filesystem::path root;

    void SetUp() override {
        auto pattern = (std::filesystem::temp_directory_path() / "toz3-unions-XXXXXX").string();
        const auto *directory = mkdtemp(pattern.data());
        ASSERT_NE(directory, nullptr);
        root = directory;
    }
    void TearDown() override {
        if (!root.empty()) std::filesystem::remove_all(root);
    }
    int compare(const char *before, const char *after) {
        const auto source = root / "source.p4";
        const auto lowered = root / "FlattenHeaderUnion.p4";
        std::ofstream(source) << before;
        std::ofstream(lowered) << after;
        auto &options = P4CContextWithOptions<CompilerOptions>::get().options();
        return process_programs({source, lowered}, &options, true, false);
    }
};

TEST_F(HeaderUnionComparison, FlattenedInputsPreserveDistinctValidityAndTheLegalInputDomain) {
    EXPECT_EQ(compare(R"(
        header E {} header H { bit<8> x; }
        header_union U { E a; H b; }
        struct Headers { E first; U u; H last; }
        control C(inout Headers h, out bool result, out bit<8> unspecified) {
            apply { result = h.u.isValid(); }
        }
        control Proto(inout Headers h, out bool result, out bit<8> unspecified);
        package Top(Proto c); Top(C()) main;
    )",
                      R"(
        header E {} header H { bit<8> x; }
        struct Headers { E first; E a; H b; H last; }
        control C(inout Headers h, out bool result, out bit<8> unspecified) {
            apply { result = h.a.isValid() != h.b.isValid(); unspecified = 0; }
        }
        control Proto(inout Headers h, out bool result, out bit<8> unspecified);
        package Top(Proto c); Top(C()) main;
    )"),
              EXIT_SUCCESS);
}

TEST_F(HeaderUnionComparison, FlatteningCannotHideAValidityOnlyViolation) {
    EXPECT_EQ(compare(R"(
        header E {} header_union U { E a; E b; }
        control C(out U u) { apply { u.a.setValid(); u.b.setInvalid(); } }
        control Proto(out U u); package Top(Proto c); Top(C()) main;
    )",
                      R"(
        header E {} struct U { E a; E b; }
        control C(out U u) { apply { u.a.setValid(); u.b.setInvalid(); } }
        control Proto(out U u); package Top(Proto c); Top(C()) main;
    )"),
              EXIT_VIOLATION);
}

}  // namespace
}  // namespace P4::ToZ3
