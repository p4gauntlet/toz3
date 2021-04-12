#ifndef TOZ3_V2_COMMON_TYPE_SIMPLE_H_
#define TOZ3_V2_COMMON_TYPE_SIMPLE_H_
#include <cstdio>

#include <map>      // std::map
#include <string>   // std::to_string
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_base.h"

namespace TOZ3_V2 {

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
        : P4Z3Instance(new IR::Type_Void()), state_vars(state_vars) {}
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
        for (auto tuple : state_vars) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second.to_string().c_str();
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

class NumericVal : public P4Z3Instance {
 protected:
    const P4State *state;
    z3::expr val;

 public:
    explicit NumericVal(const P4State *state, const IR::Type *p4_type,
                        z3::expr val)
        : P4Z3Instance(p4_type), state(state), val(val) {}

    const z3::expr *get_val() const { return &val; }

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
        : P4Z3Instance(other), state(other.state), val(other.val) {}
};

class Z3Bitvector : public NumericVal {
 public:
    bool is_signed;
    explicit Z3Bitvector(const P4State *state, const IR::Type *p4_type,
                         z3::expr val, bool is_signed = false)
        : NumericVal(state, p4_type, val), is_signed(is_signed) {}

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
    Z3Result concat(const P4Z3Instance &) const override;
    Z3Result cast(z3::sort &) const override;
    Z3Result cast(const IR::Type *) const override;
    P4Z3Instance *cast_allocate(const IR::Type *) const override;
    /****** TERNARY OPERANDS ******/
    Z3Result slice(const P4Z3Instance &, const P4Z3Instance &) const override;

    void merge(const z3::expr &cond, const P4Z3Instance &other) override;
    Z3Bitvector *copy() const override;

    cstring get_static_type() const override { return "Z3Bitvector"; }
    cstring to_string() const override {
        cstring ret = "Z3Bitvector(";
        return ret + val.to_string().c_str() + ")";
    }
    Z3Bitvector(const Z3Bitvector &other)
        : NumericVal(other), is_signed(other.is_signed) {}
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
    explicit Z3Int(const P4State *state, z3::expr val);
    explicit Z3Int(const P4State *state, int64_t int_val);
    explicit Z3Int(const P4State *state, big_int int_val);
    explicit Z3Int(const P4State *state);

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
    Z3Result concat(const P4Z3Instance &) const override;
    Z3Result cast(z3::sort &) const override;
    Z3Result cast(const IR::Type *) const override;
    P4Z3Instance *cast_allocate(const IR::Type *) const override;
    /****** TERNARY OPERANDS ******/
    Z3Result slice(const P4Z3Instance &, const P4Z3Instance &) const override;

    void merge(const z3::expr &cond, const P4Z3Instance &other) override;
    Z3Int *copy() const override;

    cstring get_static_type() const override { return "Z3Int"; }
    cstring to_string() const override {
        cstring ret = "Z3Int(";
        return ret + val.to_string().c_str() + ")";
    }

    Z3Int(const Z3Int &other) : NumericVal(other) {}
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

class DeclarationInstance : public P4Z3Instance {
 private:
    P4State *state;
    std::map<cstring, P4Z3Function> member_functions;
    ordered_map<cstring, P4Z3Instance *> members;
    // A wrapper class for table declarations
 public:
    const IR::Type_Declaration *decl;
    // constructor
    explicit DeclarationInstance(P4State *state,
                                 const IR::Type_Declaration *decl);
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override{};
    // TODO: This is a little pointless....
    DeclarationInstance *copy() const override {
        return new DeclarationInstance(state, decl);
    }

    P4Z3Instance *get_member(cstring name) const override {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    P4Z3Function get_function(cstring name) const {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }
    void apply(Visitor *, const IR::Vector<IR::Argument> *);

    cstring get_static_type() const override { return "DeclarationInstance"; }
    cstring to_string() const override {
        cstring ret = "DeclarationInstance(";
        return ret + decl->toString() + ")";
    }
};

}  // namespace TOZ3_V2

#endif  // TOZ3_V2_COMMON_TYPE_SIMPLE_H_
