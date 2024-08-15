#include "boolean_pruner.h"

#include <cstdlib>

#include "toz3/pruner/src/constants.h"
#include "toz3/pruner/src/pruner_util.h"

namespace P4::ToZ3::Pruner {

const IR::Node *rand_bool_literal() {
    auto decision = PrunerRng::get_rnd_pct();
    return new IR::BoolLiteral(decision < BOOL_PROB);
}

const IR::Node *BoolExpressionPruner::postorder(IR::LAnd * /*expr*/) { return rand_bool_literal(); }
const IR::Node *BoolExpressionPruner::postorder(IR::Neq * /*expr*/) { return rand_bool_literal(); }
const IR::Node *BoolExpressionPruner::postorder(IR::Lss * /*expr*/) { return rand_bool_literal(); }
const IR::Node *BoolExpressionPruner::postorder(IR::Leq * /*expr*/) { return rand_bool_literal(); }
const IR::Node *BoolExpressionPruner::postorder(IR::Grt * /*expr*/) { return rand_bool_literal(); }
const IR::Node *BoolExpressionPruner::postorder(IR::Geq * /*expr*/) { return rand_bool_literal(); }

const IR::Node *BoolExpressionPruner::postorder(IR::LOr * /*expr*/) { return rand_bool_literal(); }

const IR::Node *BoolExpressionPruner::postorder(IR::Equ * /*expr*/) { return rand_bool_literal(); }

const IR::P4Program *remove_bool_expressions(const IR::P4Program *temp) {
    // Removes all the nodes it receives from the vector.
    auto *bool_expression_pruner = new BoolExpressionPruner();
    temp = temp->apply(*bool_expression_pruner);
    return temp;
}

const IR::P4Program *prune_bool_expressions(const IR::P4Program *program,
                                            PrunerConfig pruner_conf) {
    int same_before_pruning = 0;
    int result = 0;
    INFO("\nReducing boolean expressions")
    for (int i = 0; i < PRUNER_MAX_ITERS; i++) {
        const auto *temp = program;
        temp = remove_bool_expressions(temp);
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

}  // namespace P4::ToZ3::Pruner
