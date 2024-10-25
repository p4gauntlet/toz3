
#ifndef _EXPRESSION_PRUNER_H
#define _EXPRESSION_PRUNER_H
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

class ExpressionPruner : public Transform {
 public:
    ExpressionPruner() {
        visitDagOnce = true;
        setName("Pruner");
    }

    const IR::Node *postorder(IR::Neg *expr) override;
    const IR::Node *postorder(IR::Cmpl *expr) override;
    const IR::Node *postorder(IR::Mul *expr) override;
    const IR::Node *postorder(IR::Div *expr) override;
    const IR::Node *postorder(IR::Add *expr) override;
    const IR::Node *postorder(IR::AddSat *expr) override;
    const IR::Node *postorder(IR::Sub *expr) override;
    const IR::Node *postorder(IR::SubSat *expr) override;
    const IR::Node *postorder(IR::BAnd *expr) override;
    const IR::Node *postorder(IR::BOr *expr) override;
    const IR::Node *postorder(IR::BXor *expr) override;

    /* these are shifts, we should only use the left-hand value */
    const IR::Node *postorder(IR::Mod *expr) override;
    const IR::Node *postorder(IR::Shl *expr) override;
    const IR::Node *postorder(IR::Shr *expr) override;

    // Trying to implement select
    const IR::Node *postorder(IR::SelectExpression *expr) override;

    // To be Implemented -----------

    // const IR::Node *postorder(const IR::Range *r);
    // const IR::Node *postorder(const IR::Cast *c);
    // const IR::Node *postorder(const IR::Concat *c);
    // const IR::Node *postorder(const IR::Slice *s);
    // const IR::Node *postorder(const IR::Mux *m);
    // const IR::Node *postorder(const IR::Member *m);
    // const IR::Node *postorder(IR::Member *p) override;
    // const IR::Node *postorder(const IR::SerEnumMember *m);
    // const IR::Node *postorder(const IR::DefaultExpression *de);
    // const IR::Node *postorder(const IR::ListExpression *le);
    // const IR::Node *postorder(const IR::TypeNameExpression *tn);
    // const IR::Node *postorder(const IR::NamedExpression *ne);
    // const IR::Node *postorder(const IR::StructExpression *sie);
    // const IR::Node *postorder(const IR::ConstructorCallExpression *cc);
    // const IR::Node *postorder(const IR::MethodCallExpression *mce);
    // const IR::Node *postorder(const IR::StructExpression *s);
};

const IR::P4Program *prune_expressions(const IR::P4Program *program, PrunerConfig pruner_conf);

}  // namespace P4::ToZ3::Pruner

#endif /* _EXPRESSION_PRUNER_H */
