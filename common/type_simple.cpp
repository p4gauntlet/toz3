#include "type_simple.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3 {

z3::expr pure_bv_cast(const z3::expr &expr, const z3::sort &dest_type) {
    // TODO: Clean this up
    uint64_t expr_size = 0;
    auto cast_expr = expr;
    if (expr.is_bv()) {
        expr_size = expr.get_sort().bv_size();
    } else if (expr.is_int()) {
        return z3::int2bv(dest_type.bv_size(), expr).simplify();
    } else if (expr.is_bool()) {
        auto *ctx = &expr.get_sort().ctx();
        expr_size = 1;
        cast_expr = z3::ite(expr, ctx->bv_val(1, 1), ctx->bv_val(0, 1));
    } else {
        BUG("Casting %s to a bit vector is not supported.",
            expr.to_string().c_str());
    }
    // At this point we are only dealing with expr bit vectors
    auto dest_size = dest_type.bv_size();

    if (expr_size < dest_size) {
        // The target value is larger, extend with zeros
        return z3::zext(cast_expr, dest_size - expr_size);
    }
    if (expr_size > dest_size) {
        // The target value is smaller, truncate everything on the right
        return cast_expr.extract(dest_size - 1, 0);
    }
    // Nothing to do just return
    return cast_expr;
}

z3::expr align_bitvectors(const P4Z3Instance *target, const z3::sort &bv_cast,
                          bool align_bv = false, cstring op = "") {
    const z3::expr *cast_expr = nullptr;
    if (const auto *target_int = target->to<Z3Int>()) {
        auto cast_val = pure_bv_cast(*target_int->get_val(), bv_cast);
        cast_expr = &cast_val;
    } else if (const auto *target_expr = target->to<Z3Bitvector>()) {
        if (align_bv) {
            auto cast_val = pure_bv_cast(*target_expr->get_val(), bv_cast);
            cast_expr = &cast_val;
        } else {
            cast_expr = target_expr->get_val();
        }
    } else {
        P4C_UNIMPLEMENTED("%s: Alignment not implemented for %s.", op,
                          target->get_static_type());
    }
    return *cast_expr;
}

/***
===============================================================================
Z3Bitvector
===============================================================================
***/

/****** UNARY OPERANDS ******/

Z3Result Z3Bitvector::operator-() const {
    return Z3Bitvector(state, p4_type, -val, is_signed);
}

Z3Result Z3Bitvector::operator~() const {
    return Z3Bitvector(state, p4_type, ~val, is_signed);
}

Z3Result Z3Bitvector::operator!() const {
    return Z3Bitvector(state, p4_type, !val, is_signed);
}

/****** BINARY OPERANDS ******/

Z3Result Z3Bitvector::operator*(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "*");
    return Z3Bitvector(state, p4_type, val * other_expr, is_signed);
}

Z3Result Z3Bitvector::operator/(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "/");
    if (is_signed) {
        return Z3Bitvector(state, p4_type, val / other_expr, is_signed);
    }
    return Z3Bitvector(state, p4_type, z3::udiv(val, other_expr), is_signed);
}

Z3Result Z3Bitvector::operator%(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "%");
    return Z3Bitvector(state, p4_type, z3::urem(val, other_expr), is_signed);
}

Z3Result Z3Bitvector::operator+(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "+");
    return Z3Bitvector(state, p4_type, val + other_expr, is_signed);
}

Z3Result Z3Bitvector::operatorAddSat(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "|+|");
    auto no_overflow = z3::bvadd_no_overflow(val, other_expr, false);
    auto no_underflow = z3::bvadd_no_underflow(val, other_expr);
    auto sort = val.get_sort();
    big_int max_return = pow((big_int)2, sort.bv_size()) - 1;
    auto *ctx = &sort.ctx();
    cstring big_str = Util::toString(max_return, 0, false);
    z3::expr max_val = ctx->bv_val(big_str.c_str(), sort.bv_size());
    return Z3Bitvector(
        state, p4_type,
        z3::ite(no_underflow && no_overflow, val + other_expr, max_val),
        is_signed);
}

Z3Result Z3Bitvector::operator-(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "!=");
    return Z3Bitvector(state, p4_type, val - other_expr, is_signed);
}

Z3Result Z3Bitvector::operatorSubSat(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "|-|");
    auto no_overflow = z3::bvsub_no_overflow(val, other_expr);
    auto no_underflow = z3::bvsub_no_underflow(val, other_expr, false);
    auto sort = val.get_sort();
    auto *ctx = &sort.ctx();
    z3::expr min_val = ctx->bv_val(0, sort.bv_size());
    return Z3Bitvector(
        state, p4_type,
        z3::ite(no_underflow && no_overflow, val - other_expr, min_val),
        is_signed);
}

Z3Result Z3Bitvector::operator>>(const P4Z3Instance &other) const {
    const z3::expr *cast_other = nullptr;
    const z3::expr *cast_this = nullptr;
    auto this_sort = val.get_sort();
    if (const auto *target_int = other.to<Z3Int>()) {
        auto cast_val = pure_bv_cast(*target_int->get_val(), this_sort);
        cast_other = &cast_val;
        cast_this = &val;
    } else if (const auto *other_expr = other.to<Z3Bitvector>()) {
        auto other_sort = other_expr->val.get_sort();
        if (other_sort.bv_size() < this_sort.bv_size()) {
            auto cast_val = pure_bv_cast(other_expr->val, this_sort);
            cast_other = &cast_val;
            cast_this = &val;
        } else {
            auto cast_val = pure_bv_cast(val, other_sort);
            cast_this = &cast_val;
            cast_other = &other_expr->val;
        }
    } else {
        P4C_UNIMPLEMENTED(">> not implemented for %s.",
                          other.get_static_type());
    }
    if (is_signed) {
        auto shift_result = z3::ashr(*cast_this, *cast_other);
        return Z3Bitvector(state, p4_type,
                           pure_bv_cast(shift_result, this_sort), is_signed);
    }
    auto shift_result = z3::lshr(*cast_this, *cast_other);
    return Z3Bitvector(state, p4_type, pure_bv_cast(shift_result, this_sort),
                       is_signed);
}

Z3Result Z3Bitvector::operator<<(const P4Z3Instance &other) const {
    const z3::expr *cast_other = nullptr;
    const z3::expr *cast_this = nullptr;
    auto this_sort = val.get_sort();
    if (const auto *target_int = other.to<Z3Int>()) {
        // Produce a zero for ints that are larger than the target width
        // TODO: Check big int here
        if (target_int->get_val()->get_numeral_int64() > this_sort.bv_size()) {
            auto bv_val = this_sort.ctx().bv_val(0, this_sort.bv_size());
            return Z3Bitvector(state, p4_type, bv_val, is_signed);
        }
        auto cast_val = pure_bv_cast(*target_int->get_val(), this_sort);
        cast_other = &cast_val;
        cast_this = &val;
    } else if (const auto *other_expr = other.to<Z3Bitvector>()) {
        auto other_sort = other_expr->val.get_sort();
        if (other_sort.bv_size() < this_sort.bv_size()) {
            auto cast_val = pure_bv_cast(other_expr->val, this_sort);
            cast_other = &cast_val;
            cast_this = &val;
        } else {
            auto cast_val = pure_bv_cast(val, other_sort);
            cast_this = &cast_val;
            cast_other = &other_expr->val;
        }
    } else {
        P4C_UNIMPLEMENTED("<< not implemented for %s.",
                          other.get_static_type());
    }
    auto shift_result = z3::shl(*cast_this, *cast_other).simplify();

    return Z3Bitvector(state, p4_type, pure_bv_cast(shift_result, this_sort),
                       is_signed);
}

z3::expr Z3Bitvector::operator==(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "==");
    // Mismatched bit vectors evaluate to false.
    // TODO: Check if this is allowed and clean this up.
    if (val.get_sort().is_bv() && other_expr.get_sort().is_bv() &&
        val.get_sort().bv_size() != other_expr.get_sort().bv_size()) {
        return state->get_z3_ctx()->bool_val(false);
    }
    return val == other_expr;
}

z3::expr Z3Bitvector::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

z3::expr Z3Bitvector::operator<(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "<");
    if (is_signed) {
        return val < other_expr;
    }
    return z3::ult(val, other_expr);
}

z3::expr Z3Bitvector::operator<=(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "<=");
    if (is_signed) {
        return val <= other_expr;
    }
    return z3::ule(val, other_expr);
}

z3::expr Z3Bitvector::operator>(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, ">");
    if (is_signed) {
        return val > other_expr;
    }
    return z3::ugt(val, other_expr);
}

z3::expr Z3Bitvector::operator>=(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, ">=");
    if (is_signed) {
        return val >= other_expr;
    }
    return z3::uge(val, other_expr);
}

z3::expr Z3Bitvector::operator&&(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "&&");
    return val && other_expr;
}

z3::expr Z3Bitvector::operator||(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "||");
    return val || other_expr;
}

Z3Result Z3Bitvector::operator&(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "&");
    return Z3Bitvector(state, p4_type, val & other_expr, is_signed);
}

Z3Result Z3Bitvector::operator|(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "|");
    return Z3Bitvector(state, p4_type, val | other_expr, is_signed);
}

Z3Result Z3Bitvector::operator^(const P4Z3Instance &other) const {
    auto other_expr = align_bitvectors(&other, val.get_sort(), false, "^");
    return Z3Bitvector(state, p4_type, val ^ other_expr, is_signed);
}

Z3Result Z3Bitvector::concat(const P4Z3Instance &other) const {
    const z3::expr *other_expr = nullptr;
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        other_expr = other_val->get_val();
        const auto *concat_type = new IR::Type_Bits(
            other_expr->get_sort().bv_size() + val.get_sort().bv_size(), false);

        return Z3Bitvector(state, concat_type, z3::concat(val, *other_expr),
                           is_signed);
    }
    P4C_UNIMPLEMENTED("concat not implemented for %s.",
                      other.get_static_type());
}

Z3Result Z3Bitvector::cast(const IR::Type *dest_type) const {
    if (const auto *tn = dest_type->to<IR::Type_Name>()) {
        dest_type = state->resolve_type(tn);
    }
    if (dest_type->equiv(*p4_type)) {
        // Nothing to do, return a copy.
        return *this;
    }
    if (const auto *tb = dest_type->to<IR::Type_Bits>()) {
        auto *ctx = &val.get_sort().ctx();
        auto dest_sort = ctx->bv_sort(tb->size);
        return Z3Bitvector(state, dest_type, pure_bv_cast(val, dest_sort));
    }
    // TODO: Merge with Bits
    if (const auto *tvb = dest_type->to<IR::Type_Varbits>()) {
        auto *ctx = &val.get_sort().ctx();
        auto dest_sort = ctx->bv_sort(tvb->size);
        return Z3Bitvector(state, dest_type, pure_bv_cast(val, dest_sort));
    }
    if (dest_type->is<IR::Type_InfInt>()) {
        // TODO: Clean this up and add some checks
        auto *ctx = &val.get_sort().ctx();
        auto dec_str = val.get_decimal_string(0);
        auto int_expr = ctx->int_val(dec_str.c_str());
        return Z3Int(state, int_expr);
    }
    if (dest_type->is<IR::Type_Boolean>()) {
        auto *ctx = &val.get_sort().ctx();
        auto dest_sort = ctx->bool_sort();
        if (val.is_bool()) {
            // nothing to do just return a new object
            return Z3Bitvector(state, &BOOL_TYPE, val);
        }
        if (val.is_bv()) {
            z3::expr bool_res = val > 0;
            return Z3Bitvector(state, &BOOL_TYPE, val > 0);
        }
    }
    if (const auto *te = dest_type->to<IR::Type_Enum>()) {
        auto new_enum = *state->find_var(te->name.name)->to<EnumInstance>();
        // TODO: Cast.
        new_enum.set_enum_val(val);
        return new_enum;
    }
    if (const auto *te = dest_type->to<IR::Type_Error>()) {
        auto new_enum = *state->find_var(te->name.name)->to<ErrorInstance>();
        // TODO: Cast.
        new_enum.set_enum_val(val);
        return new_enum;
    }
    if (const auto *te = dest_type->to<IR::Type_SerEnum>()) {
        auto new_enum = *state->find_var(te->name.name)->to<SerEnumInstance>();
        // TODO: Cast.
        new_enum.set_enum_val(val);
        return new_enum;
    }
    P4C_UNIMPLEMENTED("cast to %s not implemented for %s.",
                      dest_type->node_type_name(), get_static_type());
}

P4Z3Instance *Z3Bitvector::cast_allocate(const IR::Type *dest_type) const {
    auto cast_result = cast(dest_type);
    if (auto *z3_bit = boost::get<Z3Bitvector>(&cast_result)) {
        return z3_bit->copy();
    }
    if (auto *z3_int = boost::get<Z3Int>(&cast_result)) {
        return z3_int->copy();
    }
    if (auto *enum_instance = boost::get<EnumInstance>(&cast_result)) {
        return enum_instance->copy();
    }
    if (auto *enum_instance = boost::get<ErrorInstance>(&cast_result)) {
        return enum_instance->copy();
    }
    if (auto *enum_instance = boost::get<SerEnumInstance>(&cast_result)) {
        return enum_instance->copy();
    }
    BUG("Unexpected cast result for type %s!", dest_type->node_type_name());
}

/****** TERNARY OPERANDS ******/

Z3Result Z3Bitvector::slice(const z3::expr &hi, const z3::expr &lo) const {
    auto hi_int = hi.simplify().get_numeral_uint64();
    auto lo_int = lo.simplify().get_numeral_uint64();
    const auto *slice_type = new IR::Type_Bits(hi_int - lo_int + 1, false);
    return Z3Bitvector(state, slice_type,
                       val.extract(hi_int, lo_int).simplify(), is_signed);
}

Z3Bitvector *Z3Bitvector::copy() const {
    return new Z3Bitvector(state, p4_type, val, is_signed);
}

void Z3Bitvector::merge(const z3::expr &cond, const P4Z3Instance &then_expr) {
    if (const auto *then_expr_var = then_expr.to<Z3Bitvector>()) {
        val = z3::ite(cond, then_expr_var->val, val);
    } else if (const auto *then_expr_var = then_expr.to<Z3Int>()) {
        z3::expr cast_val =
            pure_bv_cast(*then_expr_var->get_val(), val.get_sort());
        val = z3::ite(cond, cast_val, val);
    } else {
        P4C_UNIMPLEMENTED(
            "Z3Bitvector: Merge with %s of type %s not supported.",
            then_expr.to_string(), then_expr.get_static_type());
    }
}

/***
===============================================================================
Z3Int
===============================================================================
***/

Z3Int::Z3Int(const P4State *state, const z3::expr &val)
    : NumericVal(state, &INT_TYPE, val) {}
Z3Int::Z3Int(const P4State *state, big_int int_val)
    : NumericVal(state, &INT_TYPE,
                 state->get_z3_ctx()->int_val(
                     Util::toString(std::move(int_val), 0, false))) {}

Z3Int::Z3Int(const P4State *state, int64_t int_val)
    : NumericVal(state, &INT_TYPE, state->get_z3_ctx()->int_val(int_val)) {}

Z3Int::Z3Int(const P4State *state)
    : NumericVal(state, &INT_TYPE, state->get_z3_ctx()->int_val(0)) {}

Z3Int *Z3Int::copy() const { return new Z3Int(state, val); }

void Z3Int::merge(const z3::expr &cond, const P4Z3Instance &then_expr) {
    if (const auto *then_expr_var = then_expr.to<Z3Int>()) {
        val = z3::ite(cond, then_expr_var->val, val);
    } else if (const auto *then_expr_var = then_expr.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, then_expr_var->get_val()->get_sort());
        val = z3::ite(cond, *then_expr_var->get_val(), cast_val);
    } else {
        BUG("Unsupported merge class: %s", &then_expr);
    }
}

Z3Result Z3Int::operator-() const { return Z3Int(state, -val); }

/****** BINARY OPERANDS ******/

Z3Result Z3Int::operator*(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return Z3Int(state, val * other_int->val);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val * *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("* not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator/(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return Z3Int(state, val / other_int->val);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           z3::udiv(cast_val, *other_val->get_val()));
    }
    P4C_UNIMPLEMENTED("/ not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator%(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return Z3Int(state, val % other_int->val);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           z3::urem(cast_val, *other_val->get_val()));
    }
    P4C_UNIMPLEMENTED("% not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator+(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return Z3Int(state, val + other_int->val);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val + *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("+ not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operatorAddSat(const P4Z3Instance &other) const {
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        auto no_overflow =
            z3::bvadd_no_overflow(cast_val, *other_val->get_val(), false);
        auto no_underflow =
            z3::bvadd_no_underflow(cast_val, *other_val->get_val());
        auto sort = cast_val.get_sort();
        big_int max_return = pow((big_int)2, sort.bv_size()) - 1;
        auto *ctx = &sort.ctx();
        cstring big_str = Util::toString(max_return, 0, false);
        z3::expr max_val = ctx->bv_val(big_str.c_str(), sort.bv_size());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           z3::ite(no_underflow && no_overflow,
                                   cast_val + *other_val->get_val(), max_val));
    }
    P4C_UNIMPLEMENTED("|+| not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator-(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return Z3Int(state, val - other_int->val);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val - *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("- not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operatorSubSat(const P4Z3Instance &) const {
    P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
}

Z3Result Z3Int::operator>>(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        // TODO: Figure out big int here, why is that not supported?
        auto left = val.simplify().get_numeral_int64();
        auto right = other_int->val.simplify().get_numeral_int64();
        auto result = left >> right;
        return Z3Int(state, result);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        z3::expr cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           z3::lshr(cast_val, *other_val->get_val()));
    }
    P4C_UNIMPLEMENTED(">> not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator<<(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        // TODO: Figure out big int here, why is that not supported?
        auto left = val.simplify().get_numeral_int64();
        auto right = other_int->val.simplify().get_numeral_int64();
        auto result = left << right;
        return Z3Int(state, result);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        z3::expr cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           z3::shl(cast_val, *other_val->get_val()));
    }
    P4C_UNIMPLEMENTED("<< not implemented for %s.", other.get_static_type());
}

z3::expr Z3Int::operator==(const P4Z3Instance &other) const {
    const z3::expr *this_expr = nullptr;
    const z3::expr *other_expr = nullptr;

    if (const auto *other_int = other.to<Z3Int>()) {
        this_expr = &val;
        other_expr = &other_int->val;
    } else if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        this_expr = &cast_val;
        other_expr = other_val->get_val();
    } else {
        P4C_UNIMPLEMENTED("== not implemented for %s.",
                          other.get_static_type());
    }
    return *this_expr == *other_expr;
}

z3::expr Z3Int::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

z3::expr Z3Int::operator<(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return val < other_int->val;
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        return *other_val > *this;
    }
    P4C_UNIMPLEMENTED("< not implemented for %s.", other.get_static_type());
}

z3::expr Z3Int::operator<=(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return val <= other_int->val;
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        return *other_val >= *this;
    }
    P4C_UNIMPLEMENTED("<= not implemented for %s.", other.get_static_type());
}

z3::expr Z3Int::operator>(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return val > other_int->val;
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        return *other_val < *this;
    }
    P4C_UNIMPLEMENTED("> not implemented for %s.", other.get_static_type());
}

z3::expr Z3Int::operator>=(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        return val >= other_int->val;
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        return *other_val <= *this;
    }
    P4C_UNIMPLEMENTED(">= not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator&(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        auto left = big_int(val.simplify().get_decimal_string(0));
        auto right = big_int(other_int->val.simplify().get_decimal_string(0));
        auto result = left & right;
        return Z3Int(state, result);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val & *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("| not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator|(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        auto left = big_int(val.simplify().get_decimal_string(0));
        auto right = big_int(other_int->val.simplify().get_decimal_string(0));
        auto result = left | right;
        return Z3Int(state, result);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val | *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("| not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::operator^(const P4Z3Instance &other) const {
    if (const auto *other_int = other.to<Z3Int>()) {
        auto left = big_int(val.simplify().get_decimal_string(0));
        auto right = big_int(other_int->val.simplify().get_decimal_string(0));
        auto result = left ^ right;
        return Z3Int(state, result);
    }
    if (const auto *other_val = other.to<Z3Bitvector>()) {
        auto cast_val = pure_bv_cast(val, other_val->get_val()->get_sort());
        return Z3Bitvector(state, other_val->get_p4_type(),
                           cast_val ^ *other_val->get_val());
    }
    P4C_UNIMPLEMENTED("^ not implemented for %s.", other.get_static_type());
}

Z3Result Z3Int::cast(const IR::Type *dest_type) const {
    if (const auto *tn = dest_type->to<IR::Type_Name>()) {
        dest_type = state->resolve_type(tn);
    }
    if (const auto *tb = dest_type->to<IR::Type_Bits>()) {
        // TODO: Resolve this
        auto dest_sort = state->get_z3_ctx()->bv_sort(tb->size);
        return Z3Bitvector(state, tb, pure_bv_cast(val, dest_sort));
    }
    if (const auto *tb = dest_type->to<IR::Type_Boolean>()) {
        return Z3Bitvector(state, tb, val != 0);
    }
    if (dest_type->is<IR::Type_InfInt>()) {
        // nothing to do, return a copy
        return *this;
    }
    P4C_UNIMPLEMENTED("cast_allocate not implemented for %s to type %s.",
                      get_static_type(), dest_type->node_type_name());
}

P4Z3Instance *Z3Int::cast_allocate(const IR::Type *dest_type) const {
    auto cast_result = cast(dest_type);
    if (auto *z3_bit = boost::get<Z3Bitvector>(&cast_result)) {
        return z3_bit->copy();
    }
    if (auto *z3_int = boost::get<Z3Int>(&cast_result)) {
        return z3_int->copy();
    }
    BUG("Unexpected cast result for type %s!", dest_type->node_type_name());
}

}  // namespace TOZ3
