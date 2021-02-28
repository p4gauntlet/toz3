#ifndef _TOZ3_SIMPLE_TYPE_H_
#define _TOZ3_SIMPLE_TYPE_H_

#include <cstdio>
#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string
#include <vector> // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"

#include "base_type.h"

namespace TOZ3_V2 {

// Forward declare state
class P4State;

z3::expr z3_cast(P4State *state, P4Z3Instance *expr, z3::sort *dest_type);
z3::expr complex_cast(P4State *state, P4Z3Instance *expr,
                      P4Z3Instance *dest_type);
z3::expr merge_z3_expr(z3::expr *cond, z3::expr *then_expr,
                       const P4Z3Instance *else_expr);

class ControlState : public P4Z3Instance {
 public:
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    ControlState(std::vector<std::pair<cstring, z3::expr>> state_vars)
        : state_vars(state_vars){};
    void merge(z3::expr *, const P4Z3Instance *) override{
        // Merge is a no-op here.
    };
    cstring get_static_type() const override { return "ControlState"; }
    cstring get_static_type() override { return "ControlState"; }
};

class P4Declaration : public P4Z3Instance {
    // A wrapper class for declarations
 public:
    const IR::Declaration *decl;
    // constructor
    P4Declaration(const IR::Declaration *decl) : decl(decl) {}
    void merge(z3::expr *, const P4Z3Instance *) override{
        // Merge is a no-op here.
    };
    cstring get_static_type() const override { return "P4Declaration"; }
    cstring get_static_type() override { return "P4Declaration"; }
};

class Z3Int : public P4Z3Instance {
 public:
    z3::expr val;
    int64_t width;
    Z3Int(z3::expr val, int64_t width) : val(val), width(width){};

    z3::expr operator==(const P4Z3Instance &other) override {
        if (auto other_int = other.to<Z3Int>()) {
            return val == other_int->val;
        } else {
            BUG("Unsupported Z3Int comparison.");
        }
    }
    void merge(z3::expr *cond, const P4Z3Instance *) override;
    cstring get_static_type() const override { return "Z3Int"; }
    cstring get_static_type() override { return "Z3Int"; }
};

class Z3Wrapper : public P4Z3Instance {
 public:
    z3::expr val;
    Z3Wrapper(z3::expr val) : val(val){};

    z3::expr operator==(const P4Z3Instance &other) override {
        if (auto other_int = other.to<Z3Int>()) {
            return val == other_int->val;
        } else if (auto other_val = other.to<Z3Wrapper>()) {
            return val == other_val->val;
        } else {
            BUG("Unsupported Z3Wrapper comparison.");
        }
    }

    z3::expr operator!() override { return !val; }
    z3::expr operator!() const override { return !val; }
    void merge(z3::expr *cond, const P4Z3Instance *) override;
    cstring get_static_type() const override { return "Z3Wrapper"; }
    cstring get_static_type() override { return "Z3Wrapper"; }
};

} // namespace TOZ3_V2

#endif // _TOZ3_SIMPLE_TYPE_H_
