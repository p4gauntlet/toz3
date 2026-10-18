#ifndef TOZ3_TESTS_GTEST_INTERPRETER_STATE_H_
#define TOZ3_TESTS_GTEST_INTERPRETER_STATE_H_

#include "frontends/common/parseInput.h"
#include "toz3/common/state.h"
#include "toz3/common/visitor_interpret.h"

namespace P4::ToZ3 {

// Declaration order keeps the compiler and Z3 contexts alive until all borrowed
// interpreter values and visitor references have been destroyed.
struct InterpreterTestState {
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};
    z3::context ctx;
    P4State state{&ctx};
    Z3Visitor visitor{&state};
};

}  // namespace P4::ToZ3

#endif  // TOZ3_TESTS_GTEST_INTERPRETER_STATE_H_
