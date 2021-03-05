#ifndef _TOZ3_CONTEXT_H_
#define _TOZ3_CONTEXT_H_

#include <cstdio>
#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir.h"

#include "type_complex.h"

namespace TOZ3_V2 {

class P4Scope {

 private:
    // maps of local values and types
    std::map<cstring, P4Declaration> static_decls;
    std::map<cstring, P4Z3Instance *> var_map;
    std::map<cstring, const IR::Type *> var_types;
    std::map<cstring, const IR::Type *> type_map;
    bool is_returned = false;
    std::vector<z3::expr> forward_conds;

 public:
    /****** GETTERS ******/
    std::map<cstring, const IR::Type *> *get_type_map() { return &type_map; }

    /****** STATIC DECLS ******/
    const P4Declaration *get_static_decl(cstring name) {
        return &static_decls.at(name);
    }
    void declare_static_decl(cstring name, const IR::Declaration *val) {

        static_decls.insert({name, P4Declaration(val)});
    }
    bool has_static_decl(cstring name) { return static_decls.count(name) > 0; }
    const std::map<cstring, P4Declaration> *get_const_decl_map() const {
        return &static_decls;
    }

    /****** VARIABLES ******/
    P4Z3Instance *get_var(cstring name) { return var_map.at(name); }
    void update_var(cstring name, P4Z3Instance *val) { var_map.at(name) = val; }
    void declare_var(cstring name, P4Z3Instance *val) {
        var_map.insert({name, val});
    }
    bool has_var(cstring name) { return var_map.count(name) > 0; }
    std::map<cstring, P4Z3Instance *> *get_var_map() { return &var_map; }
    const std::map<cstring, P4Z3Instance *> *get_const_var_map() const {
        return &var_map;
    }

    /****** TYPES ******/
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
    /****** VAR_TYPES ******/
    void add_type_to_var(cstring type_name, const IR::Type *t) {
        var_types[type_name] = t;
    }

    const IR::Type *get_type_for_var(cstring type_name) {
        return var_types.at(type_name);
    }

    /****** RETURN MANAGEMENT ******/
    bool has_returned() { return is_returned; }
    void set_returned(bool return_state) { is_returned = return_state; }

    void push_forward_cond(const z3::expr *forward_cond) {
        return forward_conds.push_back(*forward_cond);
    }
    std::vector<z3::expr> get_forward_conds() { return forward_conds; }
    void pop_forward_cond() { forward_conds.pop_back(); }
};

} // namespace TOZ3_V2

inline std::ostream &operator<<(std::ostream &out,
                                const TOZ3_V2::P4Scope &scope) {
    auto var_map = scope.get_const_var_map();
    for (auto it = var_map->begin(); it != var_map->end(); ++it) {
        const cstring name = it->first;
        const TOZ3_V2::P4Z3Instance *val = it->second;
        out << name << ": " << *val;
        if (std::next(it) != var_map->end()) {
            out << ", ";
        }
    }
    return out;
}

#endif // _TOZ3_CONTEXT_H_
