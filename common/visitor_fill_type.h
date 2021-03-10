#ifndef _TOZ3_TYPE_MAP_H_
#define _TOZ3_TYPE_MAP_H_

#include <z3++.h>

#include <map>
#include <vector>

#include "ir/ir.h"
#include "state.h"

namespace TOZ3_V2 {

class TypeVisitor : public Inspector {
 public:
    P4State *state;
    TypeVisitor(P4State *state) : state(state) { visitDagOnce = false; }

    // for initialization and ending
    Visitor::profile_t init_apply(const IR::Node *node) override;
    void end_apply(const IR::Node *node) override;

    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        FATAL_ERROR("TypeVisitor: IR Node %s not implemented!",
                    expr->node_type_name());
        return false;
    }

    bool preorder(const IR::P4Program *) override;

    /***** Types *****/
    bool preorder(const IR::Type_Package *) override;

    bool preorder(const IR::Type_StructLike *t) override;
    // bool preorder(const IR::Type_Stack *t) override;
    bool preorder(const IR::Type_Enum *) override;
    bool preorder(const IR::Type_Error *) override;
    // bool preorder(const IR::Type_SerEnum *) override;
    bool preorder(const IR::Type_Parser *) override;
    bool preorder(const IR::Type_Control *) override;

    bool preorder(const IR::Type_Extern *t) override;
    // bool preorder(const IR::Type_Method *t) override;
    bool preorder(const IR::Type_Typedef *t) override;
    // bool preorder(const IR::Type_Newtype *t) override;
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
    // bool preorder(const IR::Declaration_ID *di) override;
    bool preorder(const IR::Declaration_Variable *dv) override;
    bool preorder(const IR::Declaration_Constant *dv) override;
    bool preorder(const IR::Declaration_MatchKind *) override;
};
} // namespace TOZ3_V2

#endif // _TOZ3_TYPE_MAP_H_
