
#include <z3++.h>

#include "ir/id.h"
#include "ir/ir.h"
#include "lib/exceptions.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/type_complex.h"
#include "toz3/common/type_simple.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {

bool Z3Visitor::preorder(const IR::Member *m) {
    visit(m->expr);
    const auto *complex_class = state->get_expr_result();
    state->set_expr_result(complex_class->get_member(m->member.name));
    return false;
}

bool Z3Visitor::preorder(const IR::ArrayIndex *ai) {
    visit(ai->right);
    const auto *index = state->get_expr_result();
    const auto *val_container = index->to<ValContainer>();
    BUG_CHECK(val_container,
              "Access index of type %s not "
              "implemented for indexable types.",
              index->get_static_type());
    const auto expr = val_container->get_val()->simplify();
    visit(ai->left);
    const auto *indexable_class = state->get_expr_result<IndexableInstance>();
    state->set_expr_result(indexable_class->get_member(expr));
    return false;
}

bool Z3Visitor::preorder(const IR::Neg *expr) {
    visit(expr->expr);
    const auto *instance = state->get_expr_result();
    state->set_expr_result(-*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::Cmpl *expr) {
    visit(expr->expr);
    const auto *instance = state->get_expr_result();
    state->set_expr_result(~*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::LNot *expr) {
    visit(expr->expr);
    const auto *instance = state->get_expr_result();
    state->set_expr_result(!*instance);

    return false;
}

bool Z3Visitor::preorder(const IR::Mul *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left * *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Div *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left / *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Mod *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left % *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Add *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left + *right);
    return false;
}

bool Z3Visitor::preorder(const IR::AddSat *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(left->operatorAddSat(*right));

    return false;
}

bool Z3Visitor::preorder(const IR::Sub *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left - *right);

    return false;
}

bool Z3Visitor::preorder(const IR::SubSat *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(left->operatorSubSat(*right));

    return false;
}

bool Z3Visitor::preorder(const IR::Shl *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left << *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Shr *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left >> *right);

    return false;
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    auto &&result = *left == *right;
    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, result));

    return false;
}

bool Z3Visitor::preorder(const IR::Neq *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, *left != *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Lss *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, *left < *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Leq *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, *left <= *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Grt *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, *left > *right));

    return false;
}

bool Z3Visitor::preorder(const IR::Geq *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, *left >= *right));

    return false;
}

bool Z3Visitor::preorder(const IR::BAnd *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(*left & *right);

    return false;
}

bool Z3Visitor::preorder(const IR::BOr *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();
    state->set_expr_result(*left | *right);
    return false;
}

bool Z3Visitor::preorder(const IR::BXor *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result();
    visit(expr->right);
    const auto *right = state->get_expr_result();
    state->set_expr_result(*left ^ *right);
    return false;
}

bool Z3Visitor::preorder(const IR::LAnd *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result<Z3Bitvector>();
    if (left->get_val()->simplify().is_false() || state->has_exited()) {
        state->set_expr_result(left);
        return false;
    }
    auto old_vars = state->clone_vars();
    state->push_forward_cond(*left->get_val());
    visit(expr->right);
    state->pop_forward_cond();
    auto land_expr = *left && *state->get_expr_result();
    state->merge_vars(!*left->get_val(), old_vars);

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, land_expr));

    return false;
}

bool Z3Visitor::preorder(const IR::LOr *expr) {
    visit(expr->left);
    auto *left = state->copy_expr_result<Z3Bitvector>();
    if (left->get_val()->simplify().is_true() || state->has_exited()) {
        state->set_expr_result(left);
        return false;
    }
    auto old_vars = state->clone_vars();
    state->push_forward_cond(!*left->get_val());
    visit(expr->right);
    state->pop_forward_cond();
    auto lor_expr = *left || *state->get_expr_result();
    state->merge_vars(*left->get_val(), old_vars);

    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, lor_expr));

    return false;
}

bool Z3Visitor::preorder(const IR::Concat *c) {
    visit(c->left);
    auto *left = state->copy_expr_result();
    visit(c->right);
    const auto *right = state->get_expr_result();

    state->set_expr_result(left->concat(*right));
    return false;
}

bool Z3Visitor::preorder(const IR::Slice *sl) {
    visit(sl->e0);
    const auto *val = state->copy_expr_result<NumericVal>();
    visit(sl->e1);
    const auto hi = *state->get_expr_result<NumericVal>()->get_val();
    visit(sl->e2);
    const auto lo = *state->get_expr_result<NumericVal>()->get_val();
    state->set_expr_result(val->slice(hi, lo));
    return false;
}

bool Z3Visitor::preorder(const IR::PlusSlice *sl) {
    visit(sl->e0);
    const auto value = *state->get_expr_result<NumericVal>()->get_val();
    visit(sl->e1);
    auto offset = *state->get_expr_result<NumericVal>()->get_val();
    visit(sl->e2);
    const auto width = state->get_expr_result<NumericVal>()->get_val()->get_numeral_uint();
    if (width == 0) {
        // Z3 has no zero-width bitvectors; the only value of bit<0> is zero.
        state->set_expr_result(new Z3Int(state, 0));
        return false;
    }
    if (offset.is_bv()) offset = z3::bv2int(offset, false);
    const auto sourceWidth = value.get_sort().bv_size();
    auto shifted = z3::lshr(value, z3::int2bv(sourceWidth, offset));
    shifted = z3::ite(offset >= static_cast<int>(sourceWidth),
                      state->get_z3_ctx()->bv_val(0, sourceWidth), shifted);
    auto result = width <= sourceWidth ? shifted.extract(width - 1, 0)
                                       : z3::zext(shifted, width - sourceWidth);
    state->set_expr_result(
        new Z3Bitvector(state, IR::Type_Bits::get(width, false), result.simplify()));
    return false;
}

bool Z3Visitor::preorder(const IR::Cast *c) {
    // Resolve the type.
    const auto *resolved_type = state->resolve_type(c->destType);
    if (c->expr->is<IR::Invalid>()) {
        BUG_CHECK(resolved_type->is<IR::Type_Header>() || resolved_type->is<IR::Type_HeaderUnion>(),
                  "Invalid literal cast to %s", resolved_type);
        // Fresh headers and header unions have all members invalid.
        state->set_expr_result(state->gen_instance(cstring(UNDEF_LABEL), resolved_type));
        return false;
    }
    // Resolve the expression.
    visit(c->expr);
    const auto *resolved_expr = state->get_expr_result();
    // This creates a new copy. More reliable this way.
    state->set_expr_result(resolved_expr->cast_allocate(resolved_type));
    return false;
}

bool Z3Visitor::preorder(const IR::Mux *m) {
    // Resolve condition first.
    visit(m->e0);
    auto resolved_condition = state->get_expr_result<Z3Bitvector>()->get_val()->simplify();
    // Short circuit here.
    if (resolved_condition.is_true()) {
        visit(m->e1);
        return false;
    }
    if (resolved_condition.is_false()) {
        visit(m->e2);
        return false;
    }
    // Otherwise we need to merge.
    auto old_vars = state->clone_vars();
    state->push_forward_cond(resolved_condition);
    visit(m->e1);
    state->pop_forward_cond();
    auto then_has_exited = state->has_exited();
    VarMap then_vars;
    if (then_has_exited) {
        then_vars = old_vars;
    } else {
        then_vars = state->clone_vars();
    }
    state->set_exit(false);
    state->set_returned(false);
    auto *then_expr = state->copy_expr_result();
    state->restore_vars(old_vars);

    // Visit else expression.
    state->push_forward_cond(!resolved_condition);
    visit(m->e2);
    state->pop_forward_cond();
    auto else_has_exited = state->has_exited();
    if (else_has_exited) {
        state->restore_vars(old_vars);
    }
    // Merge the copy we received (note the ! here).
    then_expr->merge(!resolved_condition, *state->get_expr_result());

    state->merge_vars(resolved_condition, then_vars);
    state->set_expr_result(then_expr);
    state->set_exit(then_has_exited && else_has_exited);

    return false;
}

}  // namespace P4::ToZ3
