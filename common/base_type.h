#ifndef _TOZ3_BASE_TYPE_H_
#define _TOZ3_BASE_TYPE_H_

#include <z3++.h>

#include <cstdio>

#include <map>    // std::map
#include <vector> // std::vector

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

    virtual z3::expr operator==(const P4Z3Instance &);
    virtual z3::expr operator!();
    virtual z3::expr operator!() const;
    virtual void merge(z3::expr *, const P4Z3Instance *);
    virtual std::vector<std::pair<cstring, z3::expr>> get_z3_vars() const;
    virtual cstring get_static_type() = 0;
    virtual cstring get_static_type() const = 0;
};

typedef std::map<cstring, P4Z3Instance *> P4Z3Result;

} // namespace TOZ3_V2

#endif // _TOZ3_BASE_TYPE_H_
