#ifndef _TOZ3_SIMPLE_TYPE_H_
#define _TOZ3_SIMPLE_TYPE_H_

#include <cstdio>
#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string
#include <vector> // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_base.h"

namespace TOZ3_V2 {

// Forward declare state
class P4State;

class ControlState : public P4Z3Instance {
 public:
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    ControlState(std::vector<std::pair<cstring, z3::expr>> state_vars)
        : state_vars(state_vars){};
    ControlState(){};
    void merge(const z3::expr &, const P4Z3Instance &) override{
        // Merge is a no-op here.
    };
    // TODO: This is a little pointless....
    ControlState *copy() const override { return new ControlState(state_vars); }

    cstring get_static_type() const override { return "ControlState"; }
    cstring get_static_type() override { return "ControlState"; }
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
};

class P4Declaration : public P4Z3Instance {
    // A wrapper class for declarations
 public:
    const IR::Declaration *decl;
    // constructor
    P4Declaration(const IR::Declaration *decl) : decl(decl) {}
    // Merge is a no-op here.
    void merge(const z3::expr &, const P4Z3Instance &) override {}
    // TODO: This is a little pointless....
    P4Declaration *copy() const override { return new P4Declaration(decl); }

    cstring get_static_type() const override { return "P4Declaration"; }
    cstring get_static_type() override { return "P4Declaration"; }
    cstring to_string() const override {
        cstring ret = "P4Declaration(";
        return ret + decl->toString() + ")";
    }
};

class Z3Bitvector : public P4Z3Instance {
 public:
    z3::expr val;
    bool is_signed;
    Z3Bitvector(z3::expr val, bool is_signed = false)
        : val(val), is_signed(is_signed){};
    ~Z3Bitvector() {}
    Z3Bitvector() : val(z3::context().int_val(0)){};

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
    Z3Result operator==(const P4Z3Instance &other) const override;
    Z3Result operator!=(const P4Z3Instance &other) const override;
    Z3Result operator<(const P4Z3Instance &other) const override;
    Z3Result operator<=(const P4Z3Instance &other) const override;
    Z3Result operator>(const P4Z3Instance &other) const override;
    Z3Result operator>=(const P4Z3Instance &other) const override;
    Z3Result operator&(const P4Z3Instance &other) const override;
    Z3Result operator|(const P4Z3Instance &other) const override;
    Z3Result operator^(const P4Z3Instance &other) const override;
    Z3Result operator&&(const P4Z3Instance &other) const override;
    Z3Result operator||(const P4Z3Instance &other) const override;
    Z3Result concat(const P4Z3Instance &) const override;
    Z3Result cast(z3::sort &) const override;
    Z3Result cast(const IR::Type *) const override;
    P4Z3Instance *cast_allocate(z3::sort &) const override;
    P4Z3Instance *cast_allocate(const IR::Type *) const override;
    /****** TERNARY OPERANDS ******/
    Z3Result slice(uint64_t, uint64_t, P4Z3Instance &) const override;

    void merge(const z3::expr &cond, const P4Z3Instance &other) override;
    Z3Bitvector *copy() const override;
    void set_undefined() override {
        auto sort = val.get_sort();
        auto ctx = &sort.ctx();
        val = ctx->constant("undefined", sort);
    }

    cstring get_static_type() const override { return "Z3Bitvector"; }
    cstring get_static_type() override { return "Z3Bitvector"; }
    cstring to_string() const override {
        cstring ret = "Z3Bitvector(";
        return ret + val.to_string().c_str() + ")";
    }
};

class Z3Int : public P4Z3Instance {
 public:
    z3::expr val;
    Z3Int(z3::expr val) : val(val){};
    Z3Int(int64_t int_val, z3::context *ctx);
    Z3Int(big_int int_val, z3::context *ctx);

    Z3Int() : val(z3::context().int_val(0)){};

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
    Z3Result operator==(const P4Z3Instance &other) const override;
    Z3Result operator!=(const P4Z3Instance &other) const override;
    Z3Result operator<(const P4Z3Instance &other) const override;
    Z3Result operator<=(const P4Z3Instance &other) const override;
    Z3Result operator>(const P4Z3Instance &other) const override;
    Z3Result operator>=(const P4Z3Instance &other) const override;
    Z3Result operator&(const P4Z3Instance &other) const override;
    Z3Result operator|(const P4Z3Instance &other) const override;
    Z3Result operator^(const P4Z3Instance &other) const override;
    Z3Result operator&&(const P4Z3Instance &other) const override;
    Z3Result operator||(const P4Z3Instance &other) const override;
    Z3Result concat(const P4Z3Instance &) const override;
    Z3Result cast(z3::sort &) const override;
    Z3Result cast(const IR::Type *) const override;
    P4Z3Instance *cast_allocate(z3::sort &) const override;
    P4Z3Instance *cast_allocate(const IR::Type *) const override;
    /****** TERNARY OPERANDS ******/
    Z3Result slice(uint64_t, uint64_t, P4Z3Instance &) const override;

    void merge(const z3::expr &cond, const P4Z3Instance &other) override;
    Z3Int *copy() const override;
    void set_undefined() override {
        auto sort = val.get_sort();
        auto ctx = &sort.ctx();
        val = ctx->constant("undefined", sort);
    }

    cstring get_static_type() const override { return "Z3Int"; }
    cstring get_static_type() override { return "Z3Int"; }
    cstring to_string() const override {
        cstring ret = "Z3Int(";
        return ret + val.to_string().c_str() + " )";
    }
};

} // namespace TOZ3_V2

#endif // _TOZ3_SIMPLE_TYPE_H_
