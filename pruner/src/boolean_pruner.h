
#ifndef _BOOLEAN_PASS_H
#define _BOOLEAN_PASS_H
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

class BoolExpressionPruner : public Transform {
 public:
    BoolExpressionPruner() { visitDagOnce = true; }

    const IR::Node *postorder(IR::LAnd *expr) override;
    const IR::Node *postorder(IR::Neq *expr) override;
    const IR::Node *postorder(IR::Lss *expr) override;
    const IR::Node *postorder(IR::Leq *expr) override;
    const IR::Node *postorder(IR::Grt *expr) override;
    const IR::Node *postorder(IR::Geq *expr) override;
    const IR::Node *postorder(IR::LOr *expr) override;
    const IR::Node *postorder(IR::Equ *expr) override;
};

const IR::P4Program *prune_bool_expressions(const IR::P4Program *program, PrunerConfig pruner_conf);

}  // namespace P4::ToZ3::Pruner

#endif /* _BOOLEAN_PASS_H */
