#include "statement_pruner.h"

#include <algorithm>  // std::min

namespace P4PRUNER {

Pruner::Pruner(const std::vector<const IR::Statement *> &_to_prune)
    : to_prune(_to_prune) {
    setName("Pruner");
}

const IR::Node *Pruner::preorder(IR::Statement *s) {
    for (const auto &statement : to_prune) {
        if (*statement == *s) {
            // If the parent is an if statement, replace with an empty
            // statement. This is needed to avoid assertion errors.
            if (this->getParent<IR::IfStatement>() != nullptr) {
                return new IR::EmptyStatement();
            }
            return nullptr;
        }
    }
    return s;
}

const IR::Node *Pruner::preorder(IR::BlockStatement *s) {
    for (const auto *c : s->components) {
        visit(c);
    }
    return s;
}

const IR::Node *Pruner::preorder(IR::EmptyStatement * e) {
    // Don't remove empty statements if they are children of an if block.
    if (this->getParent<IR::IfStatement>() != nullptr) return e;
    return nullptr;
}

const IR::Node *Pruner::preorder(IR::ReturnStatement *s) {
    // Do not prune return statements.
    return s;
}

Visitor::profile_t Collector::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

bool Collector::preorder(const IR::Statement *s) {
    if (to_prune.size() <= max_statements &&
        (PrunerRng::get_rnd_pct() < STATEMENT_PROB)) {
        to_prune.push_back(s);
    }

    return true;
}

bool Collector::preorder(const IR::BlockStatement *s) {
    for (const auto *c : s->components) {
        visit(c);
    }
    return true;
}

std::vector<const IR::Statement *> collect_statements(const IR::P4Program *temp,
                                                      int max) {
    // An inspector that collects some statements at random.
    auto *collector = new P4PRUNER::Collector(max);
    temp->apply(*collector);
    return collector->to_prune;
}

const IR::P4Program *
remove_statements(const IR::P4Program *temp,
                  const std::vector<const IR::Statement *> &to_prune) {
    // Removes all the nodes it receives from the vector.
    auto *pruner = new P4PRUNER::Pruner(to_prune);
    temp = temp->apply(*pruner);
    return temp;
}

const IR::P4Program *prune_statements(const IR::P4Program *program,
                                      P4PRUNER::PrunerConfig pruner_conf,
                                      uint64_t prog_size) {
    int same_before_pruning = 0;
    int result = 0;
    int max_statements = prog_size / STATEMENT_SIZE_BANK_RATIO;

    INFO("\nPruning statements");
    for (int i = 0; i < PRUNER_MAX_ITERS; i++) {
        INFO("Trying with  " << max_statements << " statements");
        const auto *temp = program;
        std::vector<const IR::Statement *> to_prune =
            collect_statements(temp, max_statements);

        temp = remove_statements(temp, to_prune);
        result = check_pruned_program(&program, temp, pruner_conf);
        if (result != EXIT_SUCCESS) {
            same_before_pruning++;
            max_statements = std::max(1, max_statements / AIMD_DECREASE);
        } else {
            // Successful run, reset short-circuit.
            same_before_pruning = 0;
            max_statements += AIMD_INCREASE;
        }
        if (same_before_pruning >= NO_CHNG_ITERS) {
            break;
        }
    }
    // Done pruning.
    return program;
}

}  // namespace P4PRUNER
