#ifndef _TOZ3_STATE_H_
#define _TOZ3_STATE_H_

#include <alloca.h>
#include <bits/stdint-uintn.h>
#include <cstdio>
#include <sys/types.h>
#include <z3++.h>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir-generated.h"
#include "ir/ir.h"
#include "scope.h"
#include "z3_int.h"

namespace TOZ3_V2 {

class P4Scope {
 public:
    // a map of local values
    std::map<cstring, P4Z3Instance> value_map;
    // constructor
    P4Scope() {}

    // destructor
    ~P4Scope() {}
    // copy constructor
    P4Scope(const P4Scope &other);

    // overload = operator
    P4Scope &operator=(const P4Scope &other);
};

class P4State {
 public:
    z3::context *ctx;
    std::vector<P4ComplexInstance *> allocated_vars;
    z3::expr formula = ctx->bool_val(true);
    std::map<cstring, const IR::Type *> type_map;
    std::map<cstring, const IR::Declaration *> decl_map;
    std::vector<P4Scope *> scopes;
    P4Z3Instance return_expr = nullptr;

    P4State(z3::context *context) : ctx(context) {
        P4Scope *static_scope = new P4Scope();
        add_scope(static_scope);
    }
    ~P4State() {
        for (auto var : allocated_vars) {
            delete var;
        }
        allocated_vars.clear();
        for (auto scope : scopes) {
            delete scope;
        }
        scopes.clear();
    }

    void add_to_allocated(P4ComplexInstance *var) {
        allocated_vars.push_back(var);
    }

    P4Z3Instance gen_instance(cstring name, const IR::Type *typ,
                              uint64_t id = 0);

    std::vector<P4Scope *> get_state() { return scopes; }
    void merge_state(z3::expr cond, std::vector<P4Scope *> then_state,
                     std::vector<P4Scope *> else_state);
    void set_state(std::vector<P4Scope *> set_scopes) { scopes = set_scopes; }

    void add_scope(P4Scope *scope);

    const IR::Type *resolve_type(const IR::Type *type);
    void add_type(cstring type_name, const IR::Type *t);
    const IR::Type *get_type(cstring decl_name);

    void add_decl(cstring decl_name, const IR::Declaration *d);
    const IR::Declaration *get_decl(cstring decl_name);

    void insert_var(cstring name, P4Z3Instance var);
    P4Z3Instance find_var(cstring name, P4Scope **owner_scope);
    P4Z3Instance get_var(cstring name);
    void resolve_expr(const IR::Expression *expr);
    std::vector<P4Scope *> checkpoint();

    Z3Int *create_int(big_int value, uint64_t width) {
        auto val_string = Util::toString(value, 0, false);
        auto var = new Z3Int(ctx->int_val(val_string), width);
        add_to_allocated(var);
        return var;
    }
};

class ControlState : public P4ComplexInstance {
 public:
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    ControlState(std::vector<std::pair<cstring, z3::expr>> state_vars)
        : state_vars(state_vars){};
};

} // namespace TOZ3_V2

#endif // _TOZ3_STATE_H_
