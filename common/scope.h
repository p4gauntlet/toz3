#ifndef _TOZ3_CONTEXT_H_
#define _TOZ3_CONTEXT_H_

#include <cstdio>
#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir.h"

#include "complex_type.h"

namespace TOZ3_V2 {

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
    std::map<cstring, P4Z3Instance> *get_var_map() { return &var_map; }

    void add_type(cstring type_name, const IR::Type *t) {
        type_map[type_name] = t;
    }

    const IR::Type *get_type(cstring type_name) {
        return type_map.at(type_name);
    }

    bool has_type(cstring name) { return type_map.count(name) > 0; }

    const IR::Type *resolve_type(const IR::Type *type) {
        const IR::Type *ret_type = type;
        if (auto tn = type->to<IR::Type_Name>()) {
            cstring type_name = tn->path->name.name;
            return get_type(type_name);
        }
        return ret_type;
    }

    std::map<cstring, const IR::Type *> *get_type_map() { return &type_map; }

 private:
    // maps of local values and types
    std::map<cstring, P4Z3Instance> var_map;
    std::map<cstring, const IR::Type *> type_map;
};

} // namespace TOZ3_V2

#endif // _TOZ3_CONTEXT_H_
