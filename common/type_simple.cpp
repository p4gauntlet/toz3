#include "type_simple.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

/*
============================================================================
Z3Wrapper
============================================================================
*/

/****** UNARY OPERANDS ******/
z3::expr Z3Wrapper::operator-() const { return -val; }
z3::expr Z3Wrapper::operator~() const { return ~val; }
z3::expr Z3Wrapper::operator!() const { return !val; }
/****** BINARY OPERANDS ******/
z3::expr Z3Wrapper::operator*(const P4Z3Instance &)const {
    P4C_UNIMPLEMENTED("* not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator/(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("/ not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator%(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("% not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator+(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("+ not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operatorAddSat(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|+| not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator-(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("- not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operatorSubSat(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator>>(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED(">> not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator<<(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("<< not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator==(const P4Z3Instance &other) const {
    if (auto other_int = other.to<Z3Int>()) {
        return val == z3_bv_cast(&other_int->val, val.get_sort());
    } else if (auto other_val = other.to<Z3Wrapper>()) {
        return val == other_val->val;
    } else {
        BUG("Unsupported Z3Wrapper comparison.");
    }
}
z3::expr Z3Wrapper::operator!=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("!= not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator<(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("< not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator<=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("<= not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator>(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("> not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator>=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED(">= not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator&(const P4Z3Instance &)const {
    P4C_UNIMPLEMENTED("& not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator|(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("| not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator^(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("^ not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator&&(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("&& not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::operator||(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|| not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::concat(P4Z3Instance *) const {
    P4C_UNIMPLEMENTED("concat not implemented for %s.", get_static_type());
}
/****** TERNARY OPERANDS ******/
z3::expr Z3Wrapper::slice(uint64_t, uint64_t, P4Z3Instance *) const {
    P4C_UNIMPLEMENTED("slice not implemented for %s.", get_static_type());
}
z3::expr Z3Wrapper::mux(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("mux not implemented for %s.", get_static_type());
}

Z3Wrapper *Z3Wrapper::copy() const { return new Z3Wrapper(this->val); }

void Z3Wrapper::merge(z3::expr *cond, const P4Z3Instance *then_expr) {
    if (auto then_expr_var = then_expr->to<Z3Wrapper>()) {
        val = z3::ite(*cond, then_expr_var->val, val);
    } else if (auto then_expr_var = then_expr->to<Z3Int>()) {
        z3::expr cast_val = z3_bv_cast(&then_expr_var->val, val.get_sort());
        val = z3::ite(*cond, cast_val, val);
    } else {
        BUG("Z3 expression merge not supported.");
    }
}

/*
============================================================================
Z3INT
============================================================================
*/

Z3Int *Z3Int::copy() const { return new Z3Int(this->val, this->width); }

void Z3Int::merge(z3::expr *cond, const P4Z3Instance *then_expr) {
    if (auto then_expr_var = then_expr->to<Z3Int>()) {
        val = z3::ite(*cond, then_expr_var->val, val);
    } else if (auto then_expr_var = then_expr->to<Z3Wrapper>()) {
        z3::expr cast_val = z3_bv_cast(&val, then_expr_var->val.get_sort());
        val = z3::ite(*cond, then_expr_var->val, cast_val);
        width = then_expr_var->val.get_sort().bv_size();
    } else {
        BUG("Unsupported merge class: %s", then_expr);
    }
}

z3::expr Z3Int::operator-() const {
    P4C_UNIMPLEMENTED("- not implemented for %s", to_string());
}
z3::expr Z3Int::operator~() const {
    P4C_UNIMPLEMENTED("~ not implemented for %s", to_string());
}
z3::expr Z3Int::operator!() const {
    P4C_UNIMPLEMENTED("! not implemented for %s", to_string());
}
/****** BINARY OPERANDS ******/
z3::expr Z3Int::operator*(const P4Z3Instance &)const {
    P4C_UNIMPLEMENTED("* not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator/(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("/ not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator%(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("% not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator+(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("+ not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operatorAddSat(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|+| not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator-(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("- not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operatorSubSat(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator>>(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED(">> not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator<<(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("<< not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator==(const P4Z3Instance &other) const {
    if (auto other_int = other.to<Z3Int>()) {
        return val == other_int->val;
    } else if (auto other_val = other.to<Z3Int>()) {
        return val == other_val->val;
    } else {
        BUG("Unsupported Z3Int comparison.");
    }
}
z3::expr Z3Int::operator!=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("!= not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator<(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("< not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator<=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("<= not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator>(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("> not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator>=(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED(">= not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator&(const P4Z3Instance &)const {
    P4C_UNIMPLEMENTED("& not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator|(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("| not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator^(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("^ not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator&&(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("&& not implemented for %s.", get_static_type());
}
z3::expr Z3Int::operator||(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|| not implemented for %s.", get_static_type());
}
z3::expr Z3Int::concat(P4Z3Instance *) const {
    P4C_UNIMPLEMENTED("concat not implemented for %s.", get_static_type());
}
/****** TERNARY OPERANDS ******/
z3::expr Z3Int::slice(uint64_t, uint64_t, P4Z3Instance *) const {
    P4C_UNIMPLEMENTED("slice not implemented for %s.", get_static_type());
}
z3::expr Z3Int::mux(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("mux not implemented for %s.", get_static_type());
}

} // namespace TOZ3_V2
