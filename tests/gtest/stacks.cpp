#include <gtest/gtest.h>

#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

TEST(HeaderStacks, PushAndPopMoveMembersAndInvalidateVacatedElements) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    const auto *bits = IR::Type_Bits::get(8);
    IR::Type_Header header(IR::ID("H"_cs), {new IR::StructField(IR::ID("f"_cs), bits)});
    IR::Type_Array type(&header, new IR::Constant(3));
    StackInstance stack(&state, &type, "stack"_cs, 0);
    for (unsigned idx = 0; idx < 3; ++idx) {
        auto *element = stack.get_member(cstring(std::to_string(idx)))->to_mut<HeaderInstance>();
        element->setValid(&visitor, {});
        element->update_member("f"_cs, new Z3Bitvector(&state, bits, ctx.bv_val(idx + 1, 8)));
    }
    IR::Vector<IR::Argument> one{new IR::Argument(new IR::Constant(1))};
    stack.push_front(&visitor, &one);
    EXPECT_TRUE(stack.get_member("0"_cs)->to<HeaderInstance>()->get_valid()->is_false());
    EXPECT_EQ(stack.get_member("1"_cs)
                  ->get_member("f"_cs)
                  ->to<NumericVal>()
                  ->get_val()
                  ->get_numeral_uint(),
              1U);
    EXPECT_EQ(stack.get_member("2"_cs)
                  ->get_member("f"_cs)
                  ->to<NumericVal>()
                  ->get_val()
                  ->get_numeral_uint(),
              2U);
    stack.pop_front(&visitor, &one);
    EXPECT_EQ(stack.get_member("0"_cs)
                  ->get_member("f"_cs)
                  ->to<NumericVal>()
                  ->get_val()
                  ->get_numeral_uint(),
              1U);
    EXPECT_TRUE(stack.get_member("2"_cs)->to<HeaderInstance>()->get_valid()->is_false());
    IR::Vector<IR::Argument> excess{new IR::Argument(new IR::Constant(10))};
    stack.pop_front(&visitor, &excess);
    for (unsigned idx = 0; idx < 3; ++idx) {
        EXPECT_TRUE(stack.get_member(cstring(std::to_string(idx)))
                        ->to<HeaderInstance>()
                        ->get_valid()
                        ->is_false());
    }
}

TEST(HeaderStacks, UnionStackOperationsInvalidateAllUnionMembers) {
    z3::context ctx;
    P4State state(&ctx);
    Z3Visitor visitor(&state);
    IR::Type_Header header(IR::ID("H"_cs),
                           {new IR::StructField(IR::ID("f"_cs), IR::Type_Bits::get(8))});
    IR::Type_HeaderUnion headerUnion(IR::ID("U"_cs),
                                     {new IR::StructField(IR::ID("h"_cs), &header)});
    IR::Type_Array type(&headerUnion, new IR::Constant(2));
    StackInstance stack(&state, &type, "stack"_cs, 0);
    stack.get_member("0"_cs)->get_member("h"_cs)->to_mut<HeaderInstance>()->setValid(&visitor, {});
    IR::Vector<IR::Argument> one{new IR::Argument(new IR::Constant(1))};
    stack.push_front(&visitor, &one);
    EXPECT_TRUE(stack.get_member("0"_cs)
                    ->get_member("h"_cs)
                    ->to<HeaderInstance>()
                    ->get_valid()
                    ->is_false());
    EXPECT_TRUE(
        stack.get_member("1"_cs)->get_member("h"_cs)->to<HeaderInstance>()->get_valid()->is_true());
    stack.pop_front(&visitor, &one);
    EXPECT_TRUE(
        stack.get_member("0"_cs)->get_member("h"_cs)->to<HeaderInstance>()->get_valid()->is_true());
    EXPECT_TRUE(stack.get_member("1"_cs)
                    ->get_member("h"_cs)
                    ->to<HeaderInstance>()
                    ->get_valid()
                    ->is_false());
}

}  // namespace
}  // namespace P4::ToZ3
