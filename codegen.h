#ifndef _TOZ3_CODEGEN_H_
#define _TOZ3_CODEGEN_H_

#include <z3++.h>

#include <map>
#include <vector>

#include "ir/ir.h"

namespace TOZ3_V2 {

class CodeGenToz3 : public Inspector {
 public:
    z3::context *ctx;
    CodeGenToz3(z3::context *context) : ctx(context) {}

    z3::ast formula = ctx->bool_val(true);
    std::map<cstring, const IR::Type *> type_map;
    std::map<cstring, std::vector<const IR::Node *> *> z3_type_map;

    // for initialization and ending
    Visitor::profile_t init_apply(const IR::Node *node) override;
    void end_apply(const IR::Node *node) override;

    /***** Unimplemented z3::sort CodeGenToz3::resolve_type(IR::Type t) {
     *****/
    bool preorder(const IR::Node *expr) override { return false; }

    bool preorder(const IR::P4Program *) override;

    // /***** Types *****/
    // bool preorder(const IR::Type_Package *) override;

    bool preorder(const IR::Type_StructLike *t) override;
    bool preorder(const IR::Type_Stack *t) override;
    bool preorder(const IR::Type_Enum *) override;
    bool preorder(const IR::Type_Error *) override;
    // bool preorder(const IR::Type_SerEnum *) override;
    // bool preorder(const IR::Type_Parser *) override;
    // bool preorder(const IR::Type_Control *) override;

    // bool preorder(const IR::Type_Extern *t) override;
    // bool preorder(const IR::Type_Method *t) override;
    // bool preorder(const IR::Type_Typedef *t) override;
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
    // bool preorder(const IR::ArrayIndex *a) override;

    // /***** Statements *****/
    // bool preorder(const IR::BlockStatement *b) override;
    // bool preorder(const IR::AssignmentStatement *as) override;
    // bool preorder(const IR::MethodCallStatement *mcs) override;
    // bool preorder(const IR::IfStatement *ifs) override;
    // bool preorder(const IR::SwitchStatement *ss) override;
    // bool preorder(const IR::SwitchCase *sc) override;
    // bool preorder(const IR::EmptyStatement *) override;
    // bool preorder(const IR::ExitStatement *) override;
    // bool preorder(const IR::ReturnStatement *) override;

    // /***** Parser *****/
    // bool preorder(const IR::P4Parser *p) override;
    // bool preorder(const IR::ParserState *ps) override;
    // bool preorder(const IR::P4ValueSet *pvs) override;
    // bool preorder(const IR::SelectExpression *se) override;
    // bool preorder(const IR::SelectCase *se) override;
    // /***** Methods *****/
    // bool preorder(const IR::P4Control *c) override;
    // bool preorder(const IR::P4Action *p4action) override;
    // bool preorder(const IR::Parameter *param) override;
    // bool preorder(const IR::ParameterList *p) override;
    // bool preorder(const IR::TypeParameters *tp) override;
    // bool preorder(const IR::Argument *param) override;
    // bool preorder(const IR::Method *) override;
    // bool preorder(const IR::Function *) override;

    // /***** Tables *****/
    // bool preorder(const IR::P4Table *p4table) override;
    // bool preorder(const IR::Property *p) override;
    // bool preorder(const IR::ActionList *acl) override;
    // bool preorder(const IR::Entry *e) override;
    // bool preorder(const IR::EntriesList *el) override;
    // bool preorder(const IR::Key *key) override;
    // bool preorder(const IR::KeyElement *ke) override;
    // bool preorder(const IR::ExpressionValue *ev) override;

    // /***** Expressions *****/
    // bool preorder(const IR::Member *m) override;
    // bool preorder(const IR::SerEnumMember *m) override;
    // bool preorder(const IR::PathExpression *p) override;
    // bool preorder(const IR::DefaultExpression *) override;
    // bool preorder(const IR::ListExpression *le) override;
    // bool preorder(const IR::TypeNameExpression *) override;
    // bool preorder(const IR::NamedExpression *ne) override;
    // bool preorder(const IR::StructExpression *sie) override;
    // bool preorder(const IR::ConstructorCallExpression *) override;
    // bool preorder(const IR::MethodCallExpression *mce) override;
    // bool preorder(const IR::Constant *c) override;
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
    // bool preorder(const IR::Cast *c) override;
    // bool preorder(const IR::Concat *c) override;
    // bool preorder(const IR::Slice *s) override;
    // bool preorder(const IR::Mux *) override;

    // /***** Declarations *****/
    // bool preorder(const IR::Declaration_Instance *di) override;
    // bool preorder(const IR::Declaration_ID *di) override;
    // bool preorder(const IR::Declaration_Variable *dv) override;
    // bool preorder(const IR::Declaration_Constant *dv) override;
    // bool preorder(const IR::Declaration_MatchKind *) override;
 private:
    void fill_with_z3_sorts(std::vector<const IR::Node *> *sorts,
                            const IR::Type *t);
};
} // namespace TOZ3_V2

#endif // _TOZ3_CODEGEN_H_
