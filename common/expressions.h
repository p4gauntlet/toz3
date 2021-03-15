#ifndef _TOZ3_EXPRESSIONS_H_
#define _TOZ3_EXPRESSIONS_H_

#include <z3++.h>

#include <cstdio>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir.h"
#include "state.h"

namespace TOZ3_V2 {

class ExpressionResolver : public Inspector {
 private:
    /***** Unimplemented *****/
    bool preorder(const IR::Node *node) override {
        P4C_UNIMPLEMENTED("IR Node %s not implemented!",
                          node->node_type_name());
        return false;
    }
    Visitor::profile_t init_apply(const IR::Node *node) override;
    void end_apply(const IR::Node *) {}

    bool preorder(const IR::Member *m) override;
    // bool preorder(const IR::SerEnumMember *m) override;
    bool preorder(const IR::PathExpression *p) override;
    bool preorder(const IR::Constant *c) override;
    // bool preorder(const IR::DefaultExpression *) override;
    bool preorder(const IR::ListExpression *le) override;
    // bool preorder(const IR::TypeNameExpression *) override;
    bool preorder(const IR::NamedExpression *ne) override;
    bool preorder(const IR::StructExpression *sie) override;
    bool preorder(const IR::ConstructorCallExpression *) override;
    bool preorder(const IR::MethodCallExpression *mce) override;
    bool preorder(const IR::BoolLiteral *bl) override;
    // bool preorder(const IR::StringLiteral *str) override;

    /****** UNARY OPERANDS ******/
    bool preorder(const IR::Neg *expr) override;
    bool preorder(const IR::Cmpl *expr) override;
    bool preorder(const IR::LNot *expr) override;
    /****** BINARY OPERANDS ******/
    bool preorder(const IR::Mul *expr) override;
    bool preorder(const IR::Div *expr) override;
    bool preorder(const IR::Mod *expr) override;
    bool preorder(const IR::Add *expr) override;
    bool preorder(const IR::AddSat *expr) override;
    bool preorder(const IR::Sub *expr) override;
    bool preorder(const IR::SubSat *expr) override;
    bool preorder(const IR::Shl *expr) override;
    bool preorder(const IR::Shr *expr) override;
    bool preorder(const IR::Equ *expr) override;
    bool preorder(const IR::Neq *expr) override;
    bool preorder(const IR::Lss *expr) override;
    bool preorder(const IR::Leq *expr) override;
    bool preorder(const IR::Grt *expr) override;
    bool preorder(const IR::Geq *expr) override;
    bool preorder(const IR::BAnd *expr) override;
    bool preorder(const IR::BOr *expr) override;
    bool preorder(const IR::BXor *expr) override;
    bool preorder(const IR::LAnd *expr) override;
    bool preorder(const IR::LOr *expr) override;
    bool preorder(const IR::Concat *c) override;
    /****** TERNARY OPERANDS ******/
    // bool preorder(const IR::Mask *) override;
    // bool preorder(const IR::Range *) override;
    bool preorder(const IR::Cast *c) override;
    // bool preorder(const IR::Slice *s) override;
    bool preorder(const IR::Mux *) override;
    Inspector *stmt_resolver;

 public:
    P4State *state;
    ExpressionResolver(P4State *state, Inspector *stmt_resolver)
        : stmt_resolver(stmt_resolver), state(state) {
        visitDagOnce = false;
    }
};

VarMap merge_args_with_params(ExpressionResolver *expr_resolver,
                              const IR::Vector<IR::Argument> *args,
                              const IR::ParameterList *params);

void set_var(P4State *state, const IR::Expression *target,
             const P4Z3Instance *val);

} // namespace TOZ3_V2

#endif // _TOZ3_EXPRESSIONS_H_
