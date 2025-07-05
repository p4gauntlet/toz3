#ifndef TOZ3_COMMON_TYPE_SIMPLE_H_
#define TOZ3_COMMON_TYPE_SIMPLE_H_

#include <z3++.h>

#include <cstdint>
#include <string>  // std::to_string

#include "ir/ir.h"
#include "lib/big_int_util.h"
#include "lib/cstring.h"
#include "toz3/common/util.h"
#include "type_base.h"

namespace P4::ToZ3 {

// Forward declare state
class P4State;

z3::expr pure_bv_cast(const z3::expr &expr, const z3::sort &dest_type, bool is_signed = false);

class VoidResult : public P4Z3Instance {
 public:
    VoidResult() : P4Z3Instance(IR::Type_Void::get()) {}
    void merge(const z3::expr & /*cond*/, const P4Z3Instance & /*then_expr*/) override {
        // Merge is a no-op here.
    }
    VoidResult *copy() const override { return new VoidResult(); }
    cstring get_static_type() const override { return "VoidResult"_cs; }
    cstring to_string() const override {
        std::string ret = "VoidResult(";
        ret += ")";
        return ret;
    }
    P4Z3Instance *cast_allocate(const IR::Type * /*dest_type*/) const override {
        return new VoidResult();
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
    explicit NumericVal(const P4State *state, const IR::Type *p4_type, const z3::expr &val)
        : P4Z3Instance(p4_type), ValContainer(val), state(state) {}

    cstring get_static_type() const override { return "NumericVal"_cs; }
    cstring to_string() const override {
        std::string ret = "NumericVal(";
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
    bool is_signed;

 public:
    explicit Z3Bitvector(const P4State *state, const IR::Type *p4_type, const z3::expr &val,
                         bool is_signed = false);
    uint64_t get_width() const { return width; }
    bool bv_is_signed() const { return is_signed; }
    /****** UNARY OPERANDS ******/
    P4Z3Instance *operator-() const override;
    P4Z3Instance *operator~() const override;
    P4Z3Instance *operator!() const override;
    /****** BINARY OPERANDS ******/
    P4Z3Instance *operator*(const P4Z3Instance &other) const override;
    P4Z3Instance *operator/(const P4Z3Instance &other) const override;
    P4Z3Instance *operator%(const P4Z3Instance &other) const override;
    P4Z3Instance *operator+(const P4Z3Instance &other) const override;
    P4Z3Instance *operatorAddSat(const P4Z3Instance &other) const override;
    P4Z3Instance *operator-(const P4Z3Instance &other) const override;
    P4Z3Instance *operatorSubSat(const P4Z3Instance &other) const override;
    P4Z3Instance *operator>>(const P4Z3Instance &other) const override;
    P4Z3Instance *operator<<(const P4Z3Instance &other) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
    z3::expr operator<(const P4Z3Instance &other) const override;
    z3::expr operator<=(const P4Z3Instance &other) const override;
    z3::expr operator>(const P4Z3Instance &other) const override;
    z3::expr operator>=(const P4Z3Instance &other) const override;
    z3::expr operator&&(const P4Z3Instance &other) const override;
    z3::expr operator||(const P4Z3Instance &other) const override;
    P4Z3Instance *operator&(const P4Z3Instance &other) const override;
    P4Z3Instance *operator|(const P4Z3Instance &other) const override;
    P4Z3Instance *operator^(const P4Z3Instance &other) const override;
    P4Z3Instance *concat(const P4Z3Instance &other) const override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    /****** TERNARY OPERANDS ******/
    P4Z3Instance *slice(const z3::expr &hi, const z3::expr &lo) const override;
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    Z3Bitvector *copy() const override;

    cstring get_static_type() const override { return "Z3Bitvector"_cs; }
    cstring to_string() const override {
        std::string ret = "Z3Bitvector(";
        return ret + val.to_string().c_str() + ")";
    }
    // copy constructor
    Z3Bitvector(const Z3Bitvector &other)
        : NumericVal(other.state, other.p4_type, other.val),
          width(other.width),
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
    explicit Z3Int(const P4State *state, const big_int &int_val);
    explicit Z3Int(const P4State *state);

    P4Z3Instance *operator-() const override;
    /****** BINARY OPERANDS ******/
    P4Z3Instance *operator*(const P4Z3Instance &other) const override;
    P4Z3Instance *operator/(const P4Z3Instance &other) const override;
    P4Z3Instance *operator%(const P4Z3Instance &other) const override;
    P4Z3Instance *operator+(const P4Z3Instance &other) const override;
    P4Z3Instance *operatorAddSat(const P4Z3Instance &other) const override;
    P4Z3Instance *operator-(const P4Z3Instance &other) const override;
    P4Z3Instance *operatorSubSat(const P4Z3Instance &other) const override;
    P4Z3Instance *operator>>(const P4Z3Instance &other) const override;
    P4Z3Instance *operator<<(const P4Z3Instance &other) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
    z3::expr operator<(const P4Z3Instance &other) const override;
    z3::expr operator<=(const P4Z3Instance &other) const override;
    z3::expr operator>(const P4Z3Instance &other) const override;
    z3::expr operator>=(const P4Z3Instance &other) const override;
    P4Z3Instance *operator&(const P4Z3Instance &other) const override;
    P4Z3Instance *operator|(const P4Z3Instance &other) const override;
    P4Z3Instance *operator^(const P4Z3Instance &other) const override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    /****** TERNARY OPERANDS ******/
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    Z3Int *copy() const override;

    cstring get_static_type() const override { return "Z3Int"_cs; }
    cstring to_string() const override {
        std::string ret = "Z3Int(";
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

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_TYPE_SIMPLE_H_
