#include <z3++.h>

#include <cstdio>
#include <iostream>
#include <utility>

#include "lib/exceptions.h"

#include "visitor_interpret.h"

namespace TOZ3_V2 {

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

    auto&& result = *left == *right;
    state->set_expr_result(result);

    return false;
}

bool Z3Visitor::preorder(const IR::Neq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left != *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Lss *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left < *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Leq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left <= *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Grt *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    auto&& result = *left > *right;
    state->set_expr_result(result);

    return false;
}

bool Z3Visitor::preorder(const IR::Geq *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left >= *right);

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
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left && *right);

    return false;
}

bool Z3Visitor::preorder(const IR::LOr *expr) {
    visit(expr->left);
    auto left = state->copy_expr_result();
    visit(expr->right);
    auto right = state->get_expr_result();

    state->set_expr_result(*left || *right);

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

bool Z3Visitor::preorder(const IR::Cast *c) {
    // resolve expression
    visit(c->expr);
    auto resolved_expr = state->get_expr_result();

    state->set_expr_result(resolved_expr->cast(c->destType));
    return false;
}

} // namespace TOZ3_V2
