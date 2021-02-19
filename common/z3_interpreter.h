#ifndef _TOZ3_Z3_INTERPRETER_H_
#define _TOZ3_Z3_INTERPRETER_H_

#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "expression_resolver.h"
#include "ir/ir-generated.h"
#include "ir/ir.h"
#include "scope.h"
#include "state.h"

namespace TOZ3_V2 {

class Z3Visitor : public Inspector {
 public:
    P4State *state;
    P4Z3Result decl_result;
    Z3Visitor(P4State *state) : state(state) {
    }

    P4Z3Result get_decl_result() { return decl_result; }

 private:
    // for initialization and ending
    Visitor::profile_t init_apply(const IR::Node *node) override;
    P4Z3Instance resolve_member(const IR::Member *t);

    /***** Unimplemented *****/
    bool preorder(const IR::Node *expr) override {
        FATAL_ERROR("IR Node %s not implemented!", expr->node_type_name());
        return false;
    }

    // bool preorder(const IR::P4Program *) override;

    // /***** Statements *****/
    bool preorder(const IR::BlockStatement *b) override;
    bool preorder(const IR::AssignmentStatement *as) override;
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
    bool preorder(const IR::P4Control *c) override;
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

    // /***** Declarations *****/
    bool preorder(const IR::Declaration_Instance *di) override;
    // bool preorder(const IR::Declaration_ID *di) override;
    // bool preorder(const IR::Declaration_Variable *dv) override;
    // bool preorder(const IR::Declaration_Constant *dv) override;
    // bool preorder(const IR::Declaration_MatchKind *) override;
    void fill_with_z3_sorts(std::vector<const IR::Node *> *sorts,
                            const IR::Type *t);
    P4Z3Result merge_args_with_params(const IR::Vector<IR::Argument> *args,
                                      const IR::ParameterList *params);
};
} // namespace TOZ3_V2

#endif // _TOZ3_Z3_INTERPRETER_H_
