#ifndef _TOZ3_CONTEXT_H_
#define _TOZ3_CONTEXT_H_

#include <cstdio>
#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "boost/any.hpp"
#include "ir/ir.h"

#define BOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT
#include <boost/variant.hpp>
#include <boost/variant/get.hpp>

namespace TOZ3_V2 {

class P4ComplexInstance {
 public:
    P4ComplexInstance() {}
    template <typename T> bool is() const { return to<T>() != nullptr; }
    template <typename T> const T *to() const {
        return dynamic_cast<const T *>(this);
    }
    template <typename T> const T &as() const {
        return dynamic_cast<const T &>(*this);
    }
    virtual ~P4ComplexInstance() = default;
};

typedef boost::variant<P4ComplexInstance *, z3::expr> P4Z3Instance;
typedef std::map<cstring, P4Z3Instance> P4Z3Result;

template <typename T> T *check_complex(P4Z3Instance type) {
    try {
        P4ComplexInstance *pi = boost::get<P4ComplexInstance *>(type);
        return dynamic_cast<T *>(pi);
    } catch (boost::bad_get &) {
        return nullptr;
    }
}

class P4Scope {
 public:
    // constructor
    P4Scope() {}

    P4Z3Instance get_var(cstring name) { return var_map.at(name); }
    void update_var(cstring name, P4Z3Instance val) { var_map.at(name) = val; }
    void declare_var(cstring name, P4Z3Instance val) {
        var_map.insert({name, val});
    }

    bool has_var(cstring name) { return var_map.count(name) > 0; }

    void add_type(cstring type_name, const IR::Type *t) {
        type_map[type_name] = t;
    }

    const IR::Type *get_type(cstring type_name) {
        return type_map.at(type_name);
    }

    const IR::Type *resolve_type(const IR::Type *type) {
        const IR::Type *ret_type = type;
        if (auto tn = type->to<IR::Type_Name>()) {
            cstring type_name = tn->path->name.name;
            return get_type(type_name);
        }
        return ret_type;
    }
    bool has_type(cstring name) { return type_map.count(name) > 0; }

    std::map<cstring, const IR::Type *> *get_type_map() { return &type_map; }
    std::map<cstring, P4Z3Instance> *get_var_map() { return &var_map; }

 private:
    // a map of local values
    std::map<cstring, P4Z3Instance> var_map;
    std::map<cstring, const IR::Type *> type_map;
};

class ControlState : public P4ComplexInstance {
 public:
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    ControlState(std::vector<std::pair<cstring, z3::expr>> state_vars)
        : state_vars(state_vars){};
};

class P4Declaration : public P4ComplexInstance {
    // A wrapper class for declarations
 public:
    const IR::Declaration *decl;
    // constructor
    P4Declaration(const IR::Declaration *decl) : decl(decl) {}
};

class Z3Int : public P4ComplexInstance {
 public:
    z3::expr val;
    int64_t width;
    Z3Int(z3::expr val, int64_t width) : val(val), width(width){};
};

} // namespace TOZ3_V2

#endif // _TOZ3_CONTEXT_H_
