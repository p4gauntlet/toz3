#ifndef TOZ3_COMMON_VISITOR_INTERPRET_H_
#define TOZ3_COMMON_VISITOR_INTERPRET_H_
#include <map>
#include <utility>
#include <vector>

#include "../contrib/z3/z3++.h"

#include "ir/ir.h"
#include "state.h"

namespace TOZ3 {

class Z3Visitor : public Inspector {
 private:
    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        P4C_UNIMPLEMENTED("Node %s of type %s not implemented!", expr,
                          expr->node_type_name());
    }

    /***** Statements *****/
    bool preorder(const IR::BlockStatement *b) override;
    bool preorder(const IR::AssignmentStatement *as) override;
    bool preorder(const IR::MethodCallStatement *mcs) override;
    bool preorder(const IR::IfStatement *ifs) override;
    bool preorder(const IR::SwitchStatement *ss) override;
    // bool preorder(const IR::SwitchCase *sc) override;
    bool preorder(const IR::EmptyStatement *es) override;
    bool preorder(const IR::ExitStatement *es) override;
    bool preorder(const IR::ReturnStatement *rs) override;

    /***** Parser *****/
    // bool preorder(const IR::P4Parser *p) override;
    // bool preorder(const IR::ParserState *ps) override;
    // bool preorder(const IR::P4ValueSet *pvs) override;
    // bool preorder(const IR::SelectExpression *se) override;
    // bool preorder(const IR::SelectCase *se) override;
    /***** Methods *****/
    // bool preorder(const IR::P4Control *c) override;
    bool preorder(const IR::P4Action *a) override;
    // bool preorder(const IR::Parameter *param) override;
    // bool preorder(const IR::ParameterList *p) override;
    // bool preorder(const IR::TypeParameters *tp) override;
    // bool preorder(const IR::Argument *param) override;
    bool preorder(const IR::Method *m) override;
    bool preorder(const IR::Function *f) override;

    /***** Tables *****/
    bool preorder(const IR::P4Table *p4table) override;
    // bool preorder(const IR::Property *p) override;
    // bool preorder(const IR::ActionList *acl) override;
    // bool preorder(const IR::Entry *e) override;
    // bool preorder(const IR::EntriesList *el) override;
    // bool preorder(const IR::Key *key) override;
    // bool preorder(const IR::KeyElement *ke) override;
    // bool preorder(const IR::ExpressionValue *ev) override;

    /***** Expressions *****/
    bool preorder(const IR::Member *m) override;
    bool preorder(const IR::ArrayIndex *ai) override;
    // bool preorder(const IR::SerEnumMember *m) override;
    bool preorder(const IR::PathExpression *p) override;
    bool preorder(const IR::Constant *c) override;
    // bool preorder(const IR::DefaultExpression *) override;
    bool preorder(const IR::ListExpression *le) override;
    bool preorder(const IR::TypeNameExpression *tne) override;
    bool preorder(const IR::NamedExpression *ne) override;
    bool preorder(const IR::StructExpression *se) override;
    bool preorder(const IR::ConstructorCallExpression *cce) override;
    bool preorder(const IR::MethodCallExpression *mce) override;
    bool preorder(const IR::BoolLiteral *bl) override;
    bool preorder(const IR::StringLiteral *sl) override;

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
    bool preorder(const IR::Slice *s) override;
    bool preorder(const IR::Mux *m) override;

    /***** Declarations *****/
    bool preorder(const IR::Declaration_Instance *di) override;
    // bool preorder(const IR::Declaration_ID *di) override;
    bool preorder(const IR::Declaration_Variable *dv) override;
    bool preorder(const IR::Declaration_Constant *dc) override;
    // bool preorder(const IR::Declaration_MatchKind *) override;

 public:
    P4State *state;
    explicit Z3Visitor(P4State *state, bool gen_ctx = true) : state(state) {
        visitDagOnce = false;
        if (gen_ctx) {
            const auto ctx = Context();
            Visitor::init_apply(nullptr, &ctx);
        }
    }

    VarMap gen_state_from_instance(const IR::Declaration_Instance *di);
};
}  // namespace TOZ3

#endif  // TOZ3_COMMON_VISITOR_INTERPRET_H_
