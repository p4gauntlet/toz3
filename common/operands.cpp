#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool Z3Visitor::preorder(const IR::Cast *c) {
    // resolve expression
    visit(c->expr);
    P4Z3Instance *resolved_expr = state->get_expr_result();

    state->set_expr_result(cast(state, resolved_expr, c->destType));
    return false;
}

bool Z3Visitor::preorder(const IR::Member *m) {
    P4Z3Instance *complex_class;
    const IR::Expression *parent = m->expr;
    if (auto member = parent->to<IR::Member>()) {
        visit(member);
        complex_class = state->get_expr_result();
    } else if (auto name = parent->to<IR::PathExpression>()) {
        complex_class = state->get_var(name->path->name);
    } else {
        BUG("Parent Type  %s not implemented!", parent->node_type_name());
    }
    if (auto si = complex_class->to_mut<StructBase>()) {
        state->set_expr_result(si->get_member(m->member.name));
    } else {
        BUG("Can not cast to StructBase.");
    }

    return false;
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    P4Z3Instance *left = state->copy_expr_result();
    visit(expr->right);
    P4Z3Instance *right = state->copy_expr_result();

    auto result_wrapper = state->allocate_wrapper(*left == *right);
    state->set_expr_result(result_wrapper);

    return false;
}

bool Z3Visitor::preorder(const IR::LNot *expr) {
    visit(expr->expr);
    P4Z3Instance *instance = state->get_expr_result();
    z3::expr result = !*instance;
    state->set_expr_result(state->allocate_wrapper(result));

    return false;
}

} // namespace TOZ3_V2
