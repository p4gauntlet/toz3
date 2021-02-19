#ifndef _TOZ3_Z3_EXPR_RESOLVER_H_
#define _TOZ3_Z3_EXPR_RESOLVER_H_

#include <cstdio>
#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir-generated.h"
#include "ir/ir.h"
#include "scope.h"
#include "state.h"

namespace TOZ3_V2 {

// FIXME: Clean up this lazy circular dependency, currently using forward decl
class Z3Visitor;

class ExprResolver : public Inspector {
 public:
    P4State *state;
    Z3Visitor *stmt_visitor;
    ExprResolver(P4State *state, Z3Visitor *stmt_visitor)
        : state(state), stmt_visitor(stmt_visitor) {
        }

 private:
    // for initialization and ending
    Visitor::profile_t init_apply(const IR::Node *node) override;

    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        FATAL_ERROR("IR Node %s not implemented!", expr->node_type_name());
        return false;
    }

    // /***** Expressions *****/
    // bool preorder(const IR::Member *m) override;
    // bool preorder(const IR::SerEnumMember *m) override;
    bool preorder(const IR::PathExpression *p) override;
    bool preorder(const IR::Constant *c) override;
    // bool preorder(const IR::DefaultExpression *) override;
    // bool preorder(const IR::ListExpression *le) override;
    // bool preorder(const IR::TypeNameExpression *) override;
    // bool preorder(const IR::NamedExpression *ne) override;
    // bool preorder(const IR::StructExpression *sie) override;
    bool preorder(const IR::ConstructorCallExpression *) override;
    // bool preorder(const IR::MethodCallExpression *mce) override;
    // bool preorder(const IR::BoolLiteral *bl) override;
    // bool preorder(const IR::StringLiteral *str) override;

    // void visit_unary(const IR::Operation_Unary *);
    // void visit_binary(const IR::Operation_Binary *);
    // void visit_ternary(const IR::Operation_Ternary *);
    // bool preorder(const IR::Neg *expr) override;
    // bool preorder(const IR::Cmpl *expr) override;
    // bool preorder(const IR::LNot *expr) override;
    // bool preorder(const IR::Mul *expr) override;
    // bool preorder(const IR::Div *expr) override;
    // bool preorder(const IR::Mod *expr) override;
    // bool preorder(const IR::Add *expr) override;
    // bool preorder(const IR::AddSat *expr) override;
    // bool preorder(const IR::Sub *expr) override;
    // bool preorder(const IR::SubSat *expr) override;
    // bool preorder(const IR::Shl *expr) override;
    // bool preorder(const IR::Shr *expr) override;
    // bool preorder(const IR::Equ *expr) override;
    // bool preorder(const IR::Neq *expr) override;
    // bool preorder(const IR::Lss *expr) override;
    // bool preorder(const IR::Leq *expr) override;
    // bool preorder(const IR::Grt *expr) override;
    // bool preorder(const IR::Geq *expr) override;
    // bool preorder(const IR::BAnd *expr) override;
    // bool preorder(const IR::BOr *expr) override;
    // bool preorder(const IR::BXor *expr) override;
    // bool preorder(const IR::LAnd *expr) override;
    // bool preorder(const IR::LOr *expr) override;
    // bool preorder(const IR::Mask *) override;
    // bool preorder(const IR::Range *) override;
    bool preorder(const IR::Cast *c) override;
    // bool preorder(const IR::Concat *c) override;
    // bool preorder(const IR::Slice *s) override;
    // bool preorder(const IR::Mux *) override;

    P4Z3Instance cast(P4Z3Instance expr, const IR::Type *dest_type);
};
} // namespace TOZ3_V2

#endif // _TOZ3_Z3_EXPR_RESOLVER_H_
