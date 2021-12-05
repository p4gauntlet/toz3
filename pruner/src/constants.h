#ifndef _PROBABILITIES_H_
#define _PROBABILITIES_H_

// adding TEST, as it collides with constants defined by cpp.
#define EXIT_TEST_VALIDATION 20
#define EXIT_TEST_FAILURE -1
#define EXIT_TEST_SUCCESS 0
#define EXIT_TEST_UNDEFINED 30

namespace P4PRUNER {

// The maximum amount of iterations of no changes a pruner will tolerate.
constexpr int NO_CHNG_ITERS = 10;

// The maximum amount of iterations a pruner will take.
constexpr int PRUNER_MAX_ITERS = 50;
// AIDM constants
constexpr int AIMD_INCREASE = 2;
constexpr int AIMD_DECREASE = 2;

// Bool generation probabilities (whether we return false or true)
constexpr double BOOL_PROB = 0.5;

// Expression pruner probabilities.
constexpr double UNARY_EXPR_PROB = 0.5;
constexpr double BINARY_EXPR_LEFT_PROB = 0.33;
constexpr double BINARY_EXPR_RIGHT_PROB = 0.33;
constexpr double SHIFT_LEFT_PROB = 0.5;

// Replace variables probabilities.
constexpr double REPLACE_VARS_PROB = 0.5;
constexpr int REPLACE_VARS_CONST = 10;
constexpr int REPLACE_VARS_MAX_ITERS = 5;

// Statement pruner probabilities and constants
constexpr double STATEMENT_PROB = 0.5;
constexpr double IF_STATEMENT_BRANCH_PROB = 0.5;
constexpr double IF_STATEMENT_THEN_PROB = 0.25;
// The ratio of maximum statements the pruner will remove respective to the
// program size.
constexpr double STATEMENT_SIZE_BANK_RATIO = 1.1;

}  // namespace P4PRUNER

#endif /* _PROBABILITIES_H_ */
