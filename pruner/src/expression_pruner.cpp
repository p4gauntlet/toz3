#include "expression_pruner.h"

#include <cstdlib>

#include "ir/vector.h"
#include "toz3/pruner/src/constants.h"
#include "toz3/pruner/src/pruner_util.h"

namespace P4PRUNER {

const IR::Node *pick_side_binary(IR::Operation_Binary *expr) {
    auto decision = PrunerRng::get_rnd_pct();
    if (decision < BINARY_EXPR_LEFT_PROB) {
        // return the left-hand side of the expression
        return expr->left;
    }
    if (decision < BINARY_EXPR_RIGHT_PROB) {
        // return the right-hand side of the expression
        return expr->right;
    }
    // do nothing, just return the node
    return expr;
}

const IR::Node *pick_side_unary(IR::Operation_Unary *expr) {
    auto decision = PrunerRng::get_rnd_pct();
    if (decision < UNARY_EXPR_PROB) {
        // return the expression inside the operation
        return expr->expr;
    }
    return expr;
}

const IR::Node *pick_side_shift_left(IR::Operation_Binary *expr) {
    auto decision = PrunerRng::get_rnd_pct();
    if (decision < SHIFT_LEFT_PROB) {
        // return the left side of the shift
        return expr->left;
    }
    // return the unchanged operation
    return expr;
}

const IR::Node *ExpressionPruner::postorder(IR::SelectExpression *expr) {
    // Reduce the select expression to the first state in this list
    return expr->selectCases.front()->state;
}

const IR::Node *ExpressionPruner::postorder(IR::Add *expr) { return pick_side_binary(expr); }
const IR::Node *ExpressionPruner::postorder(IR::AddSat *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Sub *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::SubSat *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Mul *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Div *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::BAnd *expr) { return pick_side_binary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::BOr *expr) { return pick_side_binary(expr); }
const IR::Node *ExpressionPruner::postorder(IR::BXor *expr) { return pick_side_binary(expr); }

// Unary operations

const IR::Node *ExpressionPruner::postorder(IR::Neg *expr) { return pick_side_unary(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Cmpl *expr) { return pick_side_unary(expr); }

// Shifts

const IR::Node *ExpressionPruner::postorder(IR::Mod *expr) { return pick_side_shift_left(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Shl *expr) { return pick_side_shift_left(expr); }

const IR::Node *ExpressionPruner::postorder(IR::Shr *expr) { return pick_side_shift_left(expr); }

const IR::P4Program *remove_expressions(const IR::P4Program *temp) {
    // Removes all the nodes it receives from the vector
    auto *expression_pruner = new P4PRUNER::ExpressionPruner();
    temp = temp->apply(*expression_pruner);
    return temp;
}

const IR::P4Program *prune_expressions(const IR::P4Program *program,
                                       P4PRUNER::PrunerConfig pruner_conf) {
    int same_before_pruning = 0;
    int result = 0;
    INFO("\nPruning expressions");
    for (int i = 0; i < PRUNER_MAX_ITERS; i++) {
        const auto *temp = program;
        temp = remove_expressions(program);
        result = check_pruned_program(&program, temp, pruner_conf);
        if (result != EXIT_SUCCESS) {
            same_before_pruning++;
        } else {
            // successful run, reset short-circuit
            same_before_pruning = 0;
        }
        if (same_before_pruning >= NO_CHNG_ITERS) {
            break;
        }
    }
    // Done pruning
    return program;
}

}  // namespace P4PRUNER
