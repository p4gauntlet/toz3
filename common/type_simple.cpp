#include "type_simple.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

Z3Int *Z3Int::copy() const { return new Z3Int(this->val, this->width); }

void Z3Int::merge(z3::expr *cond, const P4Z3Instance *then_expr) {
    if (auto then_expr_var = then_expr->to<Z3Int>()) {
        val = z3::ite(*cond, then_expr_var->val, val);
    } else if (auto then_expr_var = then_expr->to<Z3Wrapper>()) {
        auto sort = then_expr_var->val.get_sort();
        z3::expr cast_val = z3_bv_cast(&val, &sort);
        val = z3::ite(*cond, then_expr_var->val, cast_val);
        width = then_expr_var->val.get_sort().bv_size();
    } else {
        BUG("Unsupported merge class: %s", then_expr);
    }
}

Z3Wrapper *Z3Wrapper::copy() const { return new Z3Wrapper(this->val); }

void Z3Wrapper::merge(z3::expr *cond, const P4Z3Instance *then_expr) {
    if (auto then_expr_var = then_expr->to<Z3Wrapper>()) {
        val = z3::ite(*cond, then_expr_var->val, val);
    } else if (auto then_expr_var = then_expr->to<Z3Int>()) {
        auto sort = val.get_sort();
        z3::expr cast_val = z3_bv_cast(&then_expr_var->val, &sort);
        val = z3::ite(*cond, cast_val, val);
    } else {
        BUG("Z3 expression merge not supported.");
    }
}

} // namespace TOZ3_V2
