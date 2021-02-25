#ifndef _TOZ3_STATE_H_
#define _TOZ3_STATE_H_

#include <sys/types.h>
#include <z3++.h>

#include <alloca.h>
#include <bits/stdint-uintn.h>
#include <cstdio>

#include <map>
#include <utility>
#include <vector>

#include "ir/ir.h"
#include "scope.h"

namespace TOZ3_V2 {

class P4State {
 public:
    z3::context *ctx;
    std::vector<P4ComplexInstance *> allocated_vars;
    z3::expr formula = ctx->bool_val(true);
    std::vector<P4Scope *> scopes;
    P4Scope *main_scope;
    P4Z3Instance return_expr = nullptr;

    P4State(z3::context *context) : ctx(context) { main_scope = new P4Scope(); }
    ~P4State() {
        for (auto var : allocated_vars) {
            delete var;
        }
        allocated_vars.clear();
        for (auto scope : scopes) {
            delete scope;
        }
        delete main_scope;
        scopes.clear();
    }

    void add_to_allocated(P4ComplexInstance *var) {
        allocated_vars.push_back(var);
    }

    P4Z3Instance gen_instance(cstring name, const IR::Type *typ,
                              uint64_t id = 0);

    std::vector<P4Scope *> get_state() { return scopes; }
    void merge_var_maps(z3::expr cond,
                        std::map<cstring, P4Z3Instance> *then_map,
                        std::map<cstring, P4Z3Instance> *else_map);

    void merge_state(z3::expr cond, std::vector<P4Scope *> then_state,
                     std::vector<P4Scope *> else_state);
    void restore_state(std::vector<P4Scope *> set_scopes) {
        scopes = set_scopes;
    }

    void push_scope();
    void pop_scope();

    const IR::Type *resolve_type(const IR::Type *type);
    void add_type(cstring type_name, const IR::Type *t);
    const IR::Type *get_type(cstring decl_name);
    const IR::Type *find_type(cstring type_name, P4Scope **owner_scope);

    void update_var(cstring name, P4Z3Instance var);
    void declare_local_var(cstring name, P4Z3Instance var);
    void declare_var(cstring name, const IR::Declaration *decl);
    P4Z3Instance find_var(cstring name, P4Scope **owner_scope);
    P4Z3Instance get_var(cstring name);
    template <typename T> T *get_var(cstring name) {
        P4Z3Instance var = get_var(name);
        if (auto cast_var = TOZ3_V2::check_complex<T>(var)) {
            return cast_var;
        } else {
            BUG("Could not cast to type %s.", typeid(T).name());
        }
    }

    void resolve_expr(const IR::Expression *expr);
    std::vector<P4Scope *> checkpoint();

    Z3Int *create_int(big_int value, uint64_t width) {
        auto val_string = Util::toString(value, 0, false);
        auto var = new Z3Int(ctx->int_val(val_string), width);
        add_to_allocated(var);
        return var;
    }
};

} // namespace TOZ3_V2

#endif // _TOZ3_STATE_H_
