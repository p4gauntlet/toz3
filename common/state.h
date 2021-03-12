#ifndef _TOZ3_STATE_H_
#define _TOZ3_STATE_H_

#include <sys/types.h>
#include <z3++.h>

#include <alloca.h>
#include <bits/stdint-uintn.h>
#include <cstdio>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ir/ir.h"
#include "scope.h"

namespace TOZ3_V2 {

typedef std::vector<P4Scope> ProgState;

class P4State {

 private:
    ProgState scopes;
    P4Scope main_scope;
    z3::context *ctx;
    Z3Bitvector z3_expr_buffer;
    Z3Int z3_int_buffer;
    P4Z3Instance *expr_result;
    bool is_exited = false;

    const IR::Type *find_type(cstring type_name, P4Scope **owner_scope);
    P4Z3Instance *find_var(cstring name, P4Scope **owner_scope);

 public:
    std::vector<std::pair<z3::expr, P4Z3Instance &>> return_exprs;
    std::vector<std::pair<z3::expr, ProgState>> return_states;
    std::vector<std::pair<z3::expr, ProgState>> exit_states;
    bool has_exited() { return is_exited; }
    void set_exit(bool exit_state) { is_exited = exit_state; }

    explicit P4State(z3::context *context)
        : ctx(context), z3_expr_buffer(this, context->bool_val(false)),
          z3_int_buffer(this, context->bool_val(false)) {
        main_scope = P4Scope();
    }
    ~P4State() {}

    /****** GETTERS ******/
    ProgState get_state() const { return scopes; }
    z3::context *get_z3_ctx() const { return ctx; }
    P4Z3Instance *get_expr_result() { return expr_result; }
    template <typename T> T *get_expr_result() {
        if (auto cast_result = expr_result->to_mut<T>()) {
            return cast_result;
        } else {
            BUG("Could not cast to type %s.", typeid(T).name());
        }
    }
    /****** ALLOCATIONS ******/
    P4Z3Instance *gen_instance(cstring name, const IR::Type *type,
                               uint64_t id = 0);
    z3::expr gen_z3_expr(cstring name, const IR::Type *type);

    /****** SCOPES AND STATES ******/
    void push_scope();
    void pop_scope();
    P4Scope *get_current_scope();
    P4Scope *get_scope_list();
    void merge_state(const z3::expr &cond, const ProgState &else_state);
    void restore_state(ProgState *set_scopes) { scopes = *set_scopes; }
    ProgState clone_state();
    ProgState fork_state();

    const std::vector<z3::expr> get_forward_conds() const {
        std::vector<z3::expr> forward_conds;
        for (auto &scope : scopes) {
            auto sub_conds = scope.get_forward_conds();
            forward_conds.insert(forward_conds.end(), sub_conds.begin(),
                                 sub_conds.end());
        }
        return forward_conds;
    }

    /****** TYPES ******/
    const IR::Type *resolve_type(const IR::Type *type) const;
    void add_type(cstring type_name, const IR::Type *t);
    const IR::Type *get_type(cstring decl_name) const;

    /****** VARIABLES ******/
    void update_var(cstring name, P4Z3Instance *var);
    void declare_var(cstring name, P4Z3Instance *var,
                     const IR::Type *decl_type);
    P4Z3Instance *get_var(cstring name);
    template <typename T> T *get_var(cstring name) {
        P4Z3Instance *var = get_var(name);
        if (auto cast_var = var->to_mut<T>()) {
            return cast_var;
        } else {
            BUG("Could not cast to type %s.", typeid(T).name());
        }
    }
    const IR::Type *get_var_type(cstring decl_name);
    /****** DECLARATIONS ******/
    void declare_static_decl(cstring name, P4Declaration *val);
    const P4Declaration *get_static_decl(cstring name);
    template <typename T> T *get_static_decl(cstring name) {
        auto decl = get_static_decl(name);
        return decl->to_mut<T>();
    }
    P4Declaration *find_static_decl(cstring name, P4Scope **owner_scope);
    /****** EXPRESSION RESULTS ******/
    P4Z3Instance *copy_expr_result() { return expr_result->copy(); }
    template <typename T> T *copy_expr_result() {
        auto intermediate = expr_result->copy();
        if (auto cast_result = intermediate->to_mut<T>()) {
            return cast_result;
        }
        BUG("Could not cast to type %s.", typeid(T).name());
    }
    void set_expr_result(P4Z3Instance *result) { expr_result = result; }
    void set_expr_result(Z3Result result) {
        if (auto result_expr = boost::get<Z3Bitvector>(&result)) {
            z3_expr_buffer = *result_expr;
            expr_result = &z3_expr_buffer;
        } else if (auto result_expr = boost::get<Z3Int>(&result)) {
            z3_int_buffer = *result_expr;
            expr_result = &z3_int_buffer;
        } else {
            P4C_UNIMPLEMENTED("Storing reference not supported");
        }
    }
};

} // namespace TOZ3_V2

inline std::ostream &operator<<(std::ostream &out,
                                const TOZ3_V2::P4State &state) {
    auto var_map = state.get_state();
    for (auto it = var_map.begin(); it != var_map.end(); ++it) {
        out << *it << " ";
    }
    return out;
}

#endif // _TOZ3_STATE_H_
