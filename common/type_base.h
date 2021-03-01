#ifndef _TOZ3_BASE_TYPE_H_
#define _TOZ3_BASE_TYPE_H_

#include <z3++.h>

#include <cstdio>

#include <map>     // std::map
#include <utility> // std::pair
#include <vector>  // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"

namespace TOZ3_V2 {
// template <class Derived>
class P4Z3Instance {
 public:
    P4Z3Instance() {}
    template <typename T> bool is() const { return to<T>() != nullptr; }
    template <typename T> const T *to() const {
        return dynamic_cast<const T *>(this);
    }
    template <typename T> T *to_mut() { return dynamic_cast<T *>(this); }
    virtual ~P4Z3Instance() = default;

    /****** UNARY OPERANDS ******/
    virtual z3::expr operator-() const {
        P4C_UNIMPLEMENTED("- not implemented for %s", to_string());
    }
    virtual z3::expr operator~() const {
        P4C_UNIMPLEMENTED("~ not implemented for %s", to_string());
    }
    virtual z3::expr operator!() const {
        P4C_UNIMPLEMENTED("! not implemented for %s", to_string());
    }
    /****** BINARY OPERANDS ******/
    virtual z3::expr operator*(const P4Z3Instance &)const {
        P4C_UNIMPLEMENTED("* not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator/(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("/ not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator%(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("% not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator+(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("+ not implemented for %s.", get_static_type());
    }
    virtual z3::expr operatorAddSat(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("|+| not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator-(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("- not implemented for %s.", get_static_type());
    }
    virtual z3::expr operatorSubSat(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator>>(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED(">> not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator<<(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("<< not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator==(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("== not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator!=(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("!= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator<(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("< not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator<=(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("<= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator>(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("> not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator>=(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED(">= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator&(const P4Z3Instance &)const {
        P4C_UNIMPLEMENTED("& not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator|(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("| not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator^(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("^ not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator&&(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("&& not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator||(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("|| not implemented for %s.", get_static_type());
    }
    virtual z3::expr concat(P4Z3Instance *) const {
        P4C_UNIMPLEMENTED("concat not implemented for %s.", get_static_type());
    }
    /****** TERNARY OPERANDS ******/
    virtual z3::expr slice(uint64_t, uint64_t, P4Z3Instance *) const {
        P4C_UNIMPLEMENTED("slice not implemented for %s.", get_static_type());
    }
    virtual z3::expr mux(const P4Z3Instance &) const {
        P4C_UNIMPLEMENTED("mux not implemented for %s.", get_static_type());
    }
    /****** CUSTOM FUNCTIONS ******/
    virtual void merge(z3::expr *, const P4Z3Instance *) {
        P4C_UNIMPLEMENTED("Complex expression merge not implemented for %s.",
                          get_static_type());
    }
    virtual std::vector<std::pair<cstring, z3::expr>> get_z3_vars() const {
        P4C_UNIMPLEMENTED("Get Z3 vars not implemented for %s.",
                          get_static_type());
    }
    virtual P4Z3Instance *copy() const {
        P4C_UNIMPLEMENTED("Copy not implemented for %s.", get_static_type());
    }

    virtual cstring get_static_type() = 0;
    virtual cstring get_static_type() const = 0;
    virtual cstring to_string() const = 0;
};

typedef std::map<cstring, P4Z3Instance *> P4Z3Result;

} // namespace TOZ3_V2

inline std::ostream &operator<<(std::ostream &out,
                                const TOZ3_V2::P4Z3Instance &type) {
    return out << type.to_string();
}

#endif // _TOZ3_BASE_TYPE_H_
