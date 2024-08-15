#ifndef TOZ3_COMMON_VISITOR_INTERPRET_H_
#define TOZ3_COMMON_VISITOR_INTERPRET_H_
#include "ir/indexed_vector.h"
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "lib/exceptions.h"
#include "state.h"

namespace P4::ToZ3 {

class DoBitFolding : public Modifier {
 private:
    P4State *state;
    void postorder(IR::Type_Bits *tb) override;
    void postorder(IR::Type_Varbits *tb) override;

 public:
    using Modifier::postorder;
    explicit DoBitFolding(P4State *state) : state(state) { visitDagOnce = false; }
};

class Z3Visitor : public Inspector {
 private:
    P4State *state;

    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        P4C_UNIMPLEMENTED("Node %s of type %s not implemented!", expr, expr->node_type_name());
    }
    // This is used for some specific behavior in exit statements
    bool in_parser = false;

    /***** Declarations *****/

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
    bool preorder(const IR::Type_Typedef *t) override;
    bool preorder(const IR::Type_Newtype *t) override;

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
    bool preorder(const IR::IndexedVector<IR::Declaration> *decls) override;
    /***** Statements *****/
    bool preorder(const IR::BlockStatement *b) override;
    bool preorder(const IR::AssignmentStatement *as) override;
    bool preorder(const IR::MethodCallStatement *mcs) override;
    bool preorder(const IR::IfStatement *ifs) override;
    bool preorder(const IR::SwitchStatement *ss) override;
    bool preorder(const IR::EmptyStatement *es) override;
    bool preorder(const IR::ExitStatement *es) override;
    bool preorder(const IR::ReturnStatement *rs) override;

    /***** Parser *****/
    bool preorder(const IR::ParserState *ps) override;

    /***** Expressions *****/
    bool preorder(const IR::Member *m) override;
    bool preorder(const IR::ArrayIndex *ai) override;
    bool preorder(const IR::PathExpression *p) override;
    bool preorder(const IR::Constant *c) override;
    bool preorder(const IR::ListExpression *le) override;
    bool preorder(const IR::TypeNameExpression *tne) override;
    bool preorder(const IR::NamedExpression *ne) override;
    bool preorder(const IR::StructExpression *se) override;
    bool preorder(const IR::ConstructorCallExpression *cce) override;
    bool preorder(const IR::MethodCallExpression *mce) override;
    bool preorder(const IR::BoolLiteral *bl) override;
    bool preorder(const IR::StringLiteral *sl) override;
    // bool preorder(const IR::DefaultExpression *) override;

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

 public:
    P4State *get_state() const { return state; }
    void set_in_parser(bool is_in_parser) { in_parser = is_in_parser; }
    bool is_in_parser() const { return in_parser; }
    explicit Z3Visitor(P4State *state, bool gen_ctx = true) : state(state) {
        visitDagOnce = false;
        if (gen_ctx) {
            const auto ctx = Context();
            Visitor::init_apply(nullptr, &ctx);
        }
    }
};
}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_VISITOR_INTERPRET_H_
