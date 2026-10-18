#include "toz3/pruner/src/extended_unused.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "frontends/common/parseInput.h"
#include "frontends/p4/toP4/toP4.h"

namespace P4::ToZ3::Pruner {
namespace {

class StructPruningTest : public ::testing::Test {
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};

 protected:
    const IR::P4Program *pruneProgram(const char *source) {
        const auto *program = parseP4String(std::string(source) + R"(
            package Top<T>(T c);
            Top(C()) main;
        )");
        if (program == nullptr) return nullptr;
        ExtendedUnusedDeclarations prune;
        program = program->apply(prune);
        EXPECT_EQ(::P4::errorCount(), 0U);
        if (program == nullptr) return nullptr;

        // Check emitted P4 with fresh types: stale IR type annotations must not
        // conceal invalid field accesses or initializer arity after pruning.
        std::ostringstream output;
        P4::ToP4 emit(&output, false);
        program->apply(emit);
        program = parseP4String(output.str());
        if (program == nullptr) return nullptr;
        P4::TypeMap typeMap;
        P4::TypeInference check(&typeMap, false);
        program = program->apply(check);
        EXPECT_EQ(::P4::errorCount(), 0U) << output.str();
        return program;
    }

    static std::vector<cstring> fields(const IR::P4Program *program, cstring name) {
        for (const auto *object : program->objects) {
            const auto *type = object->to<IR::Type_StructLike>();
            if (type == nullptr || type->name.name != name) continue;
            std::vector<cstring> result;
            for (const auto *field : type->fields) result.push_back(field->name.name);
            return result;
        }
        ADD_FAILURE() << "Missing aggregate " << name;
        return {};
    }
};

TEST_F(StructPruningTest, PrunesNestedParameterAndLocalFieldsThroughAliases) {
    const auto *program = pruneProgram(R"(
        struct Inner { bit<8> keep; bit<8> unused; }
        typedef Inner Alias;
        struct Outer { Alias nested; bit<8> unused; }
        control C(inout Outer arg, out bit<8> result) {
            Outer local;
            apply {
                local.nested.keep = arg.nested.keep;
                result = local.nested.keep;
            }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "Outer"_cs), (std::vector<cstring>{"nested"_cs}));
    EXPECT_EQ(fields(program, "Inner"_cs), (std::vector<cstring>{"keep"_cs}));
}

TEST_F(StructPruningTest, KeepsPositionalMethodArgumentLayoutsIncludingNestedTuples) {
    const auto *program = pruneProgram(R"(
        struct Inner { bit<8> a; bit<8> b; }
        struct S { Inner nested; bit<8> c; }
        extern void consume(in S value);
        control C() {
            apply { consume({ { 8w10, 8w1 }, 8w2 }); }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"nested"_cs, "c"_cs}));
    EXPECT_EQ(fields(program, "Inner"_cs), (std::vector<cstring>{"a"_cs, "b"_cs}));
}

TEST_F(StructPruningTest, KeepsVariableConstantAndAssignmentInitializerLayouts) {
    const auto *program = pruneProgram(R"(
        struct S { bit<8> a; bit<8> b; }
        control C(out bit<8> result) {
            const S constant = { 8w1, 8w2 };
            S variable = { 8w3, 8w4 };
            apply {
                result = constant.a + variable.a;
                variable = { 8w5, 8w6 };
                result = result + variable.a;
            }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"a"_cs, "b"_cs}));
}

TEST_F(StructPruningTest, KeepsNamedInitializersAndWholeValueCopiesAndReturns) {
    const auto *program = pruneProgram(R"(
        struct S { bit<8> a; bit<8> b; }
        S make() { return { a = 8w1, b = 8w2 }; }
        control C(out S result) {
            S local;
            apply { local = make(); result = local; }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"a"_cs, "b"_cs}));
}

TEST_F(StructPruningTest, PrunesIndexedHeaderFieldsAndKeepsExternMethods) {
    const auto *program = pruneProgram(R"(
        header H { bit<8> keep; bit<8> unused; }
        struct S { H[2] stack; bit<8> unused; }
        extern E { E(); void use(in bit<8> value); }
        control C(inout S arg) {
            E() ext;
            apply { ext.use(arg.stack[0].keep); }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"stack"_cs}));
    EXPECT_EQ(fields(program, "H"_cs), (std::vector<cstring>{"keep"_cs}));
}

TEST_F(StructPruningTest, RecomputesUsesAfterDeadDeclarationsDisappear) {
    const auto *program = pruneProgram(R"(
        struct S { bit<8> keep; bit<8> dead; }
        control C(in S arg, out bit<8> result) {
            bit<8> unused = arg.dead;
            apply { result = arg.keep; }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"keep"_cs}));
}

TEST_F(StructPruningTest, KeepsConstructorAndNamedActionArgumentLayouts) {
    const auto *program = pruneProgram(R"(
        struct S { bit<8> a; bit<8> b; }
        extern E { E(in S value); void use(in bit<8> value); }
        control C() {
            E({ 8w1, 8w2 }) ext;
            action consume(in S value) { ext.use(value.a); }
            apply { consume(value = { 8w3, 8w4 }); }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"a"_cs, "b"_cs}));
}

TEST_F(StructPruningTest, PrunesFieldsOfSpecializedGenericStructs) {
    const auto *program = pruneProgram(R"(
        struct S<T> { T keep; bit<8> unused; }
        control C(in S<bit<8>> arg, out bit<8> result) {
            apply { result = arg.keep; }
        }
    )");
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(fields(program, "S"_cs), (std::vector<cstring>{"keep"_cs}));
}

}  // namespace
}  // namespace P4::ToZ3::Pruner
