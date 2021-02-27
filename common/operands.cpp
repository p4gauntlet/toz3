#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "complex_type.h"
#include "scope.h"
#include "state.h"
#include "z3_interpreter.h"

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
    if (auto si = to_type<StructBase>(complex_class)) {
        state->set_expr_result(si->get_member(m->member.name));
    } else {
        BUG("Can not cast to StructBase.");
    }

    return false;
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    P4Z3Instance left = state->copy_expr_result();
    visit(expr->right);
    P4Z3Instance right = state->copy_expr_result();

    if (z3::expr *z3_left_expr = to_type<z3::expr>(&left)) {
        auto sort = z3_left_expr->get_sort();
        auto cast_expr = z3_cast(state, &right, &sort);
        state->set_expr_result(*z3_left_expr == cast_expr);
    } else if (P4ComplexInstance *z3_left_expr =
                   to_type<P4ComplexInstance>(&left)) {
        if (P4ComplexInstance *z3_right_expr =
                to_type<P4ComplexInstance>(&right)) {
            state->set_expr_result(*z3_left_expr == *z3_right_expr);
        } else {
            BUG("Unsupported equality operation for complex class.");
        }
    } else {
        BUG("Unsupported equality operation");
    }

    return false;
}

} // namespace TOZ3_V2
