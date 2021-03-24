#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool Z3Visitor::preorder(const IR::Member *m) {
    visit(m->expr);
    auto complex_class = state->get_expr_result();
    state->set_expr_result(complex_class->get_member(m->member.name));
    return false;
}

bool Z3Visitor::preorder(const IR::ArrayIndex *a) {
    visit(a->right);
    auto index = state->copy_expr_result();
    visit(a->left);
    auto stack_class = state->get_expr_result<StackInstance>();
    state->set_expr_result(stack_class->get_member(index));
    return false;
}

bool Z3Visitor::preorder(const IR::Neg *expr) {
    visit(expr->expr);
    auto instance = state->get_expr_result();
    state->set_expr_result(-*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::Cmpl *expr) {
    visit(expr->expr);
    auto instance = state->get_expr_result();
    state->set_expr_result(~*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::LNot *expr) {
    visit(expr->expr);
    auto instance = state->get_expr_result();
    state->set_expr_result(!*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::Mul *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left * *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Div *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left / *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Mod *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left % *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Add *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left + *right);
    return false;
}

bool Z3Visitor::preorder(const IR::AddSat *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(left->operatorAddSat(*right));

    return false;
}

bool Z3Visitor::preorder(const IR::Sub *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left - *right);

    return false;
}

bool Z3Visitor::preorder(const IR::SubSat *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(left->operatorSubSat(*right));

    return false;
}

bool Z3Visitor::preorder(const IR::Shl *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left << *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Shr *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left >> *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    auto &&result = *left == *right;
    state->set_expr_result(Z3Bitvector(state, result));

    return false;
}

bool Z3Visitor::preorder(const IR::Neq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left != *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Lss *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left < *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Leq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left <= *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Grt *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left > *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Geq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left >= *right));

    return false;
}

bool Z3Visitor::preorder(const IR::BAnd *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left & *right);

    return false;
}

bool Z3Visitor::preorder(const IR::BOr *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left | *right);

    return false;
}

bool Z3Visitor::preorder(const IR::BXor *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left ^ *right);

    return false;
}

bool Z3Visitor::preorder(const IR::LAnd *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result<Z3Bitvector>();
    if (left->val.simplify().is_false() or state->has_returned()) {
        state->set_expr_result(*left);
        return false;
    }
    state->push_forward_cond((left->val));
    visit(expr->right);
    state->pop_forward_cond();
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left && *right));

    return false;
}

bool Z3Visitor::preorder(const IR::LOr *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result<Z3Bitvector>();
    if (left->val.simplify().is_true() or state->has_returned()) {
        state->set_expr_result(*left);
        return false;
    }
    state->push_forward_cond(!(left->val));
    visit(expr->right);
    state->pop_forward_cond();
    auto right = state->get_expr_result();

    state->set_expr_result(Z3Bitvector(state, *left || *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Concat *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(left->concat(*right));
    return false;
}

bool Z3Visitor::preorder(const IR::Slice *sl) {
    visit(sl->e0);
    auto val = state->copy_expr_result();
    visit(sl->e1);
    auto hi = state->copy_expr_result();
    visit(sl->e2);
    auto lo = state->get_expr_result();
    state->set_expr_result(val->slice(*hi, *lo));
    return false;
}

bool Z3Visitor::preorder(const IR::Cast *c) {
    // resolve expression
    visit(c->expr);
    auto resolved_expr = state->get_expr_result();

    state->set_expr_result(resolved_expr->cast_allocate(c->destType));
    return false;
}

bool Z3Visitor::preorder(const IR::Mux *m) {
    // resolve condition first
    visit(m->e0);
    auto resolved_condition =
        state->get_expr_result<Z3Bitvector>()->val.simplify();
    // short circuit here
    if (resolved_condition.is_true()) {
        visit(m->e1);
        return false;
    } else if (resolved_condition.is_false()) {
        visit(m->e2);
        return false;
    }
    // otherwise we need to merge
    auto old_vars = state->clone_vars();
    state->push_forward_cond(resolved_condition);
    visit(m->e1);
    auto then_has_exited = state->has_exited();
    VarMap then_vars;
    if (then_has_exited) {
        then_vars = old_vars;
    } else {
        then_vars = state->clone_vars();
    }
    state->set_exit(false);
    state->set_returned(false);
    auto then_expr = state->copy_expr_result();
    state->restore_vars(old_vars);

    // visit else expression
    state->push_forward_cond(!resolved_condition);
    visit(m->e2);
    state->pop_forward_cond();
    auto else_has_exited = state->has_exited();
    if (else_has_exited) {
        state->restore_vars(old_vars);
    }
    // merge the copy we received (note the NOT here)
    then_expr->merge(!resolved_condition, *state->get_expr_result());

    state->merge_vars(resolved_condition, then_vars);
    state->set_expr_result(then_expr);
    state->set_exit(then_has_exited && else_has_exited);

    return false;
}

} // namespace TOZ3_V2
