#ifndef TOZ3_COMMON_VISITOR_FILL_TYPE_H_
#define TOZ3_COMMON_VISITOR_FILL_TYPE_H_

#include <map>
#include <vector>

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "visitor_interpret.h"

namespace TOZ3 {

class TypeVisitor : public Inspector {
    P4State *state;
    Z3Visitor resolve_expr;

 public:
    explicit TypeVisitor(P4State *state)
        : state(state), resolve_expr(Z3Visitor(state)) {
        visitDagOnce = false;
    }

    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        FATAL_ERROR("TypeVisitor: IR Node %s not implemented!",
                    expr->node_type_name());
    }

    bool preorder(const IR::P4Program *prog) override;

    /***** Types *****/
    bool preorder(const IR::Type_Package *tp) override;

    bool preorder(const IR::Type_StructLike *ts) override;
    bool preorder(const IR::Type_Enum *te) override;
    bool preorder(const IR::Type_Error *te) override;
    bool preorder(const IR::Type_SerEnum *tse) override;
    bool preorder(const IR::Type_Parser *tp) override;
    bool preorder(const IR::Type_Control *tc) override;

    bool preorder(const IR::Type_Extern *t) override;
    // bool preorder(const IR::Type_Method *t) override;
    bool preorder(const IR::Type_Typedef *t) override;
    bool preorder(const IR::Type_Newtype *t) override;
    // bool preorder(const IR::Type_Bits *t) override;
    // bool preorder(const IR::Type_Varbits *t) override;
    // bool preorder(const IR::Type_Name *t) override;
    // bool preorder(const IR::Type_Var *) override;
    // bool preorder(const IR::Type_Specialized *t) override;
    // bool preorder(const IR::Type_Boolean *t) override;
    // bool preorder(const IR::Type_Void *t) override;
    // bool preorder(const IR::Type_Tuple *t) override;
    // bool preorder(const IR::Type_InfInt *) override;
    // bool preorder(const IR::Type_Dontcare *) override;
    // bool preorder(const IR::Type_String *) override;

    bool preorder(const IR::P4Parser *p) override;
    bool preorder(const IR::P4Control *c) override;

    /***** Declarations *****/
    bool preorder(const IR::Function *f) override;
    bool preorder(const IR::Method *m) override;
    bool preorder(const IR::P4Action *a) override;
    bool preorder(const IR::P4Table *t) override;
    bool preorder(const IR::Declaration_Instance *di) override;
    bool preorder(const IR::Declaration_Variable *dv) override;
    bool preorder(const IR::Declaration_Constant *dc) override;
    bool preorder(const IR::Declaration_MatchKind *dm) override;
    bool preorder(const IR::P4ValueSet *pvs) override;
};
}  // namespace TOZ3

#endif  // TOZ3_COMMON_VISITOR_FILL_TYPE_H_
