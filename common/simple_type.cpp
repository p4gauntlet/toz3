#include "simple_type.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

z3::expr z3_bv_cast(const z3::expr *expr, z3::sort *dest_type) {
    uint64_t expr_size;
    uint64_t dest_size = dest_type->bv_size();
    if (expr->is_bv()) {
        expr_size = expr->get_sort().bv_size();
    } else if (expr->is_int()) {
        return z3::int2bv(dest_type->bv_size(), *expr);
    } else {
        BUG("Cast to z3 bit vector type not supported.");
    }

    // At this point we are only dealing with expr bit vectors

    if (expr_size < dest_size) {
        // The target value is larger, extend with zeros
        return z3::zext(*expr, dest_size - expr_size);
    } else if (expr_size > dest_size) {
        // The target value is smaller, truncate everything on the right
        return expr->extract(dest_size - 1, 0);
    } else {
        // Nothing to do just return
        return *expr;
    }
}

z3::expr z3_cast(P4State *, P4Z3Instance *expr, z3::sort *dest_type) {
    if (dest_type->is_bv()) {
        if (auto z3_var = expr->to<Z3Wrapper>()) {
            return z3_bv_cast(&z3_var->val, dest_type);
        } else if (auto z3_var = expr->to<Z3Int>()) {
            return z3_bv_cast(&z3_var->val, dest_type);
        } else {
            BUG("Cast to bit vector type not supported.");
        }
    } else {
        BUG("Cast not supported.");
    }
}

void Z3Int::merge(z3::expr *cond, const P4Z3Instance *other) {
    if (auto other_var = other->to<Z3Int>()) {
        val = z3::ite(*cond, val, other_var->val);
    } else if (auto other_var = other->to<Z3Wrapper>()) {
        auto sort = other_var->val.get_sort();
        z3::expr cast_val = z3_bv_cast(&val, &sort);
        val = z3::ite(*cond, cast_val, other_var->val);
        width = other_var->val.get_sort().bv_size();
    } else {
        BUG("Unsupported merge class: %s", other);
    }
}


void Z3Wrapper::merge(z3::expr *cond, const P4Z3Instance *other) {
    if (auto other_var = other->to<Z3Wrapper>()) {
        val = z3::ite(*cond, val, other_var->val);
    } else if (auto other_var = other->to<Z3Int>()) {
        auto sort = val.get_sort();
        z3::expr cast_val = z3_bv_cast(&other_var->val, &sort);
        val = z3::ite(*cond, val, cast_val);
    } else {
        BUG("Z3 expression merge not supported.");
    }
}

} // namespace TOZ3_V2
