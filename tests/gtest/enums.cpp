#include <gtest/gtest.h>

#include "interpreter_state.h"
#include "toz3/common/state.h"
#include "toz3/common/type_complex.h"

namespace P4::ToZ3 {
namespace {

constexpr int ENUM_WIDTH = 8;
constexpr int WIDE_ENUM_WIDTH = 16;
constexpr unsigned EXTENDED_LABEL_COUNT = 24;

TEST(Enums, ExtendedNamespacesHaveDistinctLabelsBeyondSixteenMembers) {
    z3::context ctx;
    P4State state(&ctx);
    const IR::Type_Enum type(IR::ID("E"_cs), {new IR::Declaration_ID("First"_cs)});
    EnumInstance instance(&state, &type, "e"_cs, 0);
    for (unsigned i = 1; i < EXTENDED_LABEL_COUNT; ++i) {
        const cstring name = std::to_string(i);
        instance.add_enum_member(name);
        instance.add_enum_member(name);
        const auto *label = instance.get_member(name)->to<NumericVal>();
        ASSERT_NE(label, nullptr);
        EXPECT_EQ(label->get_val()->get_sort().bv_size(), 32U);
        EXPECT_EQ(label->get_val()->get_numeral_uint(), i);
    }
}

void checkEnumWidening(P4State *state, const SerEnumInstance &value, const IR::Type_Bits *bits,
                       bool isSigned) {
    const Z3Bitvector negative(state, bits, state->get_z3_ctx()->bv_val(-1, ENUM_WIDTH), isSigned);
    const auto *instantiated = value.instantiate(negative);
    const auto *underlying = instantiated->cast_allocate(bits)->to<Z3Bitvector>();
    ASSERT_NE(underlying, nullptr);
    const auto *wide =
        underlying->cast_allocate(IR::Type_Bits::get(WIDE_ENUM_WIDTH))->to<Z3Bitvector>();
    ASSERT_NE(wide, nullptr);
    EXPECT_EQ(wide->get_val()->simplify().get_numeral_uint(), isSigned ? 65535U : 255U);
}

void checkSerializedEnum(bool isSigned) {
    z3::context ctx;
    P4State state(&ctx);
    const auto *bits = IR::Type_Bits::get(ENUM_WIDTH, isSigned);
    const IR::Type_SerEnum type(IR::ID("E"_cs), bits, {});
    SerEnumInstance value(&state, {}, &type, "e"_cs, 0);
    const auto symbolic = ctx.bv_const("e", ENUM_WIDTH);
    value.set_enum_val(symbolic);
    const Z3Bitvector numeric(&state, bits, symbolic, isSigned);
    const auto *cast = value.cast_allocate(bits)->to<Z3Bitvector>();
    ASSERT_NE(cast, nullptr);
    EXPECT_EQ(cast->bv_is_signed(), isSigned);
    EXPECT_EQ(cast->get_val()->get_sort().bv_size(), 8U);
    z3::solver solver(ctx);
    solver.add((*cast->get_val() != symbolic) || !value.operator==(numeric) ||
               !numeric.operator==(value));
    EXPECT_EQ(solver.check(), z3::unsat);
    checkEnumWidening(&state, value, bits, isSigned);
}

TEST(SerializedEnums, UnderlyingValuesPreserveWidthSignednessAndSymmetricEquality) {
    checkSerializedEnum(false);
    checkSerializedEnum(true);
}

TEST(SerializedEnums, OutputNamesJoinOnlyNonemptyComponents) {
    z3::context ctx;
    P4State state(&ctx);
    const IR::Type_SerEnum type(IR::ID("E"_cs), IR::Type_Bits::get(ENUM_WIDTH), {});
    const SerEnumInstance anonymous(&state, {}, &type, ""_cs, 0);
    const SerEnumInstance named(&state, {}, &type, "field"_cs, 0);
    EXPECT_EQ(anonymous.get_z3_vars("hdr.field"_cs).front().first, "hdr.field"_cs);
    EXPECT_EQ(named.get_z3_vars().front().first, "field"_cs);
    EXPECT_EQ(named.get_z3_vars("hdr"_cs).front().first, "hdr.field"_cs);
}

TEST(SerializedEnums, NestedOutputLabelsMatchLoweredBitFields) {
    InterpreterTestState test;
    const auto *program = parseP4String(R"(
        enum bit<8> E { A = 1 }
        struct Original { E field; }
        struct Lowered { bit<8> field; }
        struct NestedOriginal { Original inner; }
        struct NestedLowered { Lowered inner; }
    )");
    ASSERT_NE(program, nullptr);
    test.visitor.visit(program);
    auto *original = test.state.gen_instance("out"_cs, test.state.get_type("NestedOriginal"_cs))
                         ->to_mut<StructBase>();
    auto *lowered = test.state.gen_instance("out"_cs, test.state.get_type("NestedLowered"_cs))
                        ->to_mut<StructBase>();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(lowered, nullptr);
    const auto input = test.ctx.bv_const("input", ENUM_WIDTH);
    original->bind(&input, ENUM_WIDTH);
    lowered->bind(&input, ENUM_WIDTH);
    const auto originalFields = original->get_z3_vars("out"_cs);
    const auto loweredFields = lowered->get_z3_vars("out"_cs);
    ASSERT_EQ(originalFields.size(), 1U);
    ASSERT_EQ(loweredFields.size(), 1U);
    EXPECT_EQ(originalFields.front().first, "out.inner.field"_cs);
    EXPECT_EQ(originalFields.front().first, loweredFields.front().first);
    z3::solver solver(test.ctx);
    solver.add(originalFields.front().second != loweredFields.front().second);
    EXPECT_EQ(solver.check(), z3::unsat);
}

}  // namespace
}  // namespace P4::ToZ3
