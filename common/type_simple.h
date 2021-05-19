#ifndef TOZ3_COMMON_TYPE_SIMPLE_H_
#define TOZ3_COMMON_TYPE_SIMPLE_H_
#include <cstdio>

#include <map>      // std::map
#include <string>   // std::to_string
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_base.h"

namespace TOZ3 {

// Forward declare state
class P4State;

z3::expr pure_bv_cast(const z3::expr &expr, const z3::sort &dest_type);

class VoidResult : public P4Z3Instance {
 public:
    VoidResult() : P4Z3Instance(new IR::Type_Void()) {}
    void merge(const z3::expr &, const P4Z3Instance &) override{
        // Merge is a no-op here.
    };
    // TODO: This is a little pointless....
    VoidResult *copy() const override { return new VoidResult(); }

    cstring get_static_type() const override { return "VoidResult"; }
    cstring to_string() const override {
        cstring ret = "VoidResult(";
        ret += ")";
        return ret;
    }
    P4Z3Instance *cast_allocate(const IR::Type *) const override {
        // TODO: This should not be necessary.
        return new VoidResult();
    }
};

class ControlState : public P4Z3Instance {
 public:
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    explicit ControlState(std::vector<std::pair<cstring, z3::expr>> state_vars)
        : P4Z3Instance(new IR::Type_Void()), state_vars(std::move(state_vars)) {
    }
    ControlState() : P4Z3Instance(new IR::Type_Void()) {}
    void merge(const z3::expr &, const P4Z3Instance &) override{
        // Merge is a no-op here.
    };
    // TODO: This is a little pointless....
    ControlState *copy() const override { return new ControlState(state_vars); }

    cstring get_static_type() const override { return "ControlState"; }
    cstring to_string() const override {
        cstring ret = "ControlState(";
        bool first = true;
        for (const auto &tuple : state_vars) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second.to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    P4Z3Instance *cast_allocate(const IR::Type *) const override {
        // TODO: This should not be necessary.
        return new ControlState(state_vars);
    }
};

class ValContainer {
 protected:
    z3::expr val;

 public:
    explicit ValContainer(const z3::expr &val) : val(val) {}
    const z3::expr *get_val() const { return &val; }
};

class NumericVal : public P4Z3Instance, public ValContainer {
 protected:
    const P4State *state;

 public:
    explicit NumericVal(const P4State *state, const IR::Type *p4_type,
                        const z3::expr &val)
        : P4Z3Instance(p4_type), ValContainer(val), state(state) {}

    cstring get_static_type() const override { return "NumericVal"; }
    cstring to_string() const override {
        cstring ret = "NumericVal(";
        return ret + val.to_string().c_str() + ")";
    }
    void set_undefined() override {
        auto sort = val.get_sort();
        auto *ctx = &sort.ctx();
        val = ctx->constant(UNDEF_LABEL, sort);
    }
    NumericVal(const NumericVal &other)
        : P4Z3Instance(other), ValContainer(other.val), state(other.state) {}
};

class Z3Bitvector : public NumericVal {
 private:
    uint64_t width = 0;

 public:
    bool is_signed;
    explicit Z3Bitvector(const P4State *state, const IR::Type *p4_type,
                         const z3::expr &val, bool is_signed = false);
    uint64_t get_width() const { return width; }

    /****** UNARY OPERANDS ******/
    Z3Result operator-() const override;
    Z3Result operator~() const override;
    Z3Result operator!() const override;
    /****** BINARY OPERANDS ******/
    Z3Result operator*(const P4Z3Instance &other) const override;
    Z3Result operator/(const P4Z3Instance &other) const override;
    Z3Result operator%(const P4Z3Instance &other) const override;
    Z3Result operator+(const P4Z3Instance &other) const override;
    Z3Result operatorAddSat(const P4Z3Instance &other) const override;
    Z3Result operator-(const P4Z3Instance &other) const override;
    Z3Result operatorSubSat(const P4Z3Instance &other) const override;
    Z3Result operator>>(const P4Z3Instance &other) const override;
    Z3Result operator<<(const P4Z3Instance &other) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
    z3::expr operator<(const P4Z3Instance &other) const override;
    z3::expr operator<=(const P4Z3Instance &other) const override;
    z3::expr operator>(const P4Z3Instance &other) const override;
    z3::expr operator>=(const P4Z3Instance &other) const override;
    z3::expr operator&&(const P4Z3Instance &other) const override;
    z3::expr operator||(const P4Z3Instance &other) const override;
    Z3Result operator&(const P4Z3Instance &other) const override;
    Z3Result operator|(const P4Z3Instance &other) const override;
    Z3Result operator^(const P4Z3Instance &other) const override;
    Z3Result concat(const P4Z3Instance &other) const override;
    Z3Result cast(const IR::Type *dest_type) const override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    /****** TERNARY OPERANDS ******/
    Z3Result slice(const z3::expr &hi, const z3::expr &lo) const override;
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    Z3Bitvector *copy() const override;

    cstring get_static_type() const override { return "Z3Bitvector"; }
    cstring to_string() const override {
        cstring ret = "Z3Bitvector(";
        return ret + val.to_string().c_str() + ")";
    }
    Z3Bitvector(const Z3Bitvector &other)
        : NumericVal(other.state, other.p4_type, other.val), width(other.width),
          is_signed(other.is_signed) {}
    // overload = operator
    Z3Bitvector &operator=(const Z3Bitvector &other) {
        if (this == &other) {
            return *this;
        }
        this->val = other.val;
        this->state = other.state;
        this->p4_type = other.p4_type;
        this->is_signed = other.is_signed;

        return *this;
    }
};

class Z3Int : public NumericVal {
 public:
    explicit Z3Int(const P4State *state, const z3::expr &val);
    explicit Z3Int(const P4State *state, int64_t int_val);
    explicit Z3Int(const P4State *state, big_int int_val);
    explicit Z3Int(const P4State *state);

    Z3Result operator-() const override;
    /****** BINARY OPERANDS ******/
    Z3Result operator*(const P4Z3Instance &other) const override;
    Z3Result operator/(const P4Z3Instance &other) const override;
    Z3Result operator%(const P4Z3Instance &other) const override;
    Z3Result operator+(const P4Z3Instance &other) const override;
    Z3Result operatorAddSat(const P4Z3Instance &other) const override;
    Z3Result operator-(const P4Z3Instance &other) const override;
    Z3Result operatorSubSat(const P4Z3Instance &other) const override;
    Z3Result operator>>(const P4Z3Instance &other) const override;
    Z3Result operator<<(const P4Z3Instance &other) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
    z3::expr operator<(const P4Z3Instance &other) const override;
    z3::expr operator<=(const P4Z3Instance &other) const override;
    z3::expr operator>(const P4Z3Instance &other) const override;
    z3::expr operator>=(const P4Z3Instance &other) const override;
    Z3Result operator&(const P4Z3Instance &other) const override;
    Z3Result operator|(const P4Z3Instance &other) const override;
    Z3Result operator^(const P4Z3Instance &other) const override;
    Z3Result cast(const IR::Type *dest_type) const override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    /****** TERNARY OPERANDS ******/
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    Z3Int *copy() const override;

    cstring get_static_type() const override { return "Z3Int"; }
    cstring to_string() const override {
        cstring ret = "Z3Int(";
        return ret + val.to_string().c_str() + ")";
    }

    Z3Int(const Z3Int &other) = default;
    // overload = operator
    Z3Int &operator=(const Z3Int &other) {
        if (this == &other) {
            return *this;
        }
        this->val = other.val;
        this->state = other.state;
        this->p4_type = other.p4_type;

        return *this;
    }
};

// Guess I can only go so far with variants...
inline z3::expr check_eq(const Z3Result &left, const Z3Result &right) {
    if (const auto *result_left = boost::get<Z3Bitvector>(&left)) {
        if (const auto *result_right = boost::get<Z3Bitvector>(&right)) {
            return *result_left == *result_right;
        }
        if (const auto *result_right = boost::get<Z3Int>(&right)) {
            return *result_left == *result_right;
        }
    } else if (const auto *result_left = boost::get<Z3Int>(&left)) {
        if (const auto *result_right = boost::get<Z3Bitvector>(&right)) {
            return *result_left == *result_right;
        }
        if (const auto *result_right = boost::get<Z3Int>(&right)) {
            return *result_left == *result_right;
        }
    }
    P4C_UNIMPLEMENTED("Storing reference not supported");
}

}  // namespace TOZ3

#endif  // TOZ3_COMMON_TYPE_SIMPLE_H_
