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

cstring infer_name(const IR::Annotations *annots, cstring default_name);

class P4State {
 private:
    ProgState scopes;
    P4Scope main_scope = P4Scope();
    z3::context *ctx;
    Z3Bitvector z3_expr_buffer;
    Z3Int z3_int_buffer;
    P4Z3Instance *expr_result;
    bool is_exited = false;

    const IR::Type *find_type(cstring type_name, P4Scope **owner_scope);
    z3::expr exit_cond = ctx->bool_val(true);
    P4Scope *get_mut_current_scope() { return &scopes.back(); }

 public:
    const P4Scope &get_current_scope() const { return scopes.back(); }
    std::vector<std::pair<z3::expr, VarMap>> exit_states;
    bool has_exited() { return is_exited; }
    void set_exit(bool exit_state) { is_exited = exit_state; }

    explicit P4State(z3::context *context)
        : ctx(context), z3_expr_buffer(this, context->bool_val(false)),
          z3_int_buffer(this, context->bool_val(false)) {}
    ~P4State() {}

    /****** GETTERS ******/
    ProgState get_state() const { return scopes; }
    z3::context *get_z3_ctx() const { return ctx; }
    const P4Z3Instance *get_expr_result() const { return expr_result; }
    template <typename T> const T *get_expr_result() const {
        if (auto cast_result = expr_result->to<T>()) {
            return cast_result;
        } else {
            BUG("Could not cast to type %s.", typeid(T).name());
        }
    }
    /****** ALLOCATIONS ******/
    z3::expr gen_z3_expr(cstring name, const IR::Type *type);
    P4Z3Instance *gen_instance(cstring name, const IR::Type *type,
                               uint64_t id = 0);

    /****** COPY-IN/COPY-OUT ******/
    VarMap merge_args_with_params(Visitor *visitor,
                                  const IR::Vector<IR::Argument> *args,
                                  const IR::ParameterList *params);
    void copy_in(Visitor *visitor, const IR::ParameterList *params,
                 const IR::Vector<IR::Argument> *arguments);
    void copy_out();
    void set_copy_out_args(CopyArgs &out_args) {
        auto scope = get_mut_current_scope();
        scope->set_copy_out_args(out_args);
    }
    CopyArgs get_copy_out_args() const {
        auto scope = get_current_scope();
        return scope.get_copy_out_args();
    }
    /****** SCOPES AND STATES ******/
    void push_scope();
    void pop_scope();
    void merge_state(const z3::expr &cond, const ProgState &else_state);
    void restore_state(const ProgState &set_scopes) { scopes = set_scopes; }
    ProgState clone_state() const;
    VarMap get_vars() const;
    VarMap clone_vars() const;
    void restore_vars(const VarMap &input_map);
    void merge_vars(const z3::expr &cond, const VarMap &other);

    const z3::expr get_exit_cond() { return exit_cond; }

    void set_exit_cond(const z3::expr &forward_cond) {
        exit_cond = forward_cond;
    }

    const std::vector<z3::expr> get_forward_conds() const {
        std::vector<z3::expr> forward_conds;
        for (auto &scope : scopes) {
            auto sub_conds = scope.get_forward_conds();
            forward_conds.insert(forward_conds.end(), sub_conds.begin(),
                                 sub_conds.end());
        }
        return forward_conds;
    }
    void push_forward_cond(const z3::expr &forward_cond) {
        auto scope = get_mut_current_scope();
        scope->push_forward_cond(forward_cond);
    }
    void pop_forward_cond() {
        auto scope = get_mut_current_scope();
        scope->pop_forward_cond();
    }
    bool has_returned() const {
        auto scope = get_current_scope();
        return scope.has_returned();
    }
    void set_returned(bool return_state) {
        auto scope = get_mut_current_scope();
        scope->set_returned(return_state);
    }
    void push_return_expr(const z3::expr &cond, P4Z3Instance *return_expr) {
        auto scope = get_mut_current_scope();
        return scope->push_return_expr(cond, return_expr);
    }
    std::vector<std::pair<z3::expr, P4Z3Instance *>> get_return_exprs() {
        auto scope = get_mut_current_scope();
        return scope->get_return_exprs();
    }
    void push_return_state(const z3::expr &cond, const VarMap &state) {
        auto scope = get_mut_current_scope();
        return scope->push_return_state(cond, state);
    }
    std::vector<std::pair<z3::expr, VarMap>> get_return_states() const {
        auto scope = get_current_scope();
        return scope.get_return_states();
    }

    /****** TYPES ******/
    const IR::Type *resolve_type(const IR::Type *type) const;
    void add_type(cstring type_name, const IR::Type *t);
    const IR::Type *get_type(cstring decl_name) const;

    /****** VARIABLES ******/
    P4Z3Instance *find_var(cstring name, P4Scope **owner_scope);
    void update_var(cstring name, P4Z3Instance *var);
    void declare_var(cstring name, P4Z3Instance *var,
                     const IR::Type *decl_type);
    P4Z3Instance *get_var(cstring name) const;
    const IR::Type *get_var_type(cstring decl_name) const;
    void set_var(Visitor *visitor, const IR::Expression *target,
                 P4Z3Instance *rval);
    void set_var(MemberStruct *member_struct, P4Z3Instance *rval);

    /****** DECLARATIONS ******/
    void declare_static_decl(cstring name, P4Declaration *val);
    const P4Declaration *get_static_decl(cstring name) const;
    template <typename T> T *get_static_decl(cstring name) const {
        auto decl = get_static_decl(name);
        return decl->to_mut<T>();
    }
    P4Declaration *find_static_decl(cstring name, P4Scope **owner_scope);
    /****** EXPRESSION RESULTS ******/
    P4Z3Instance *copy_expr_result() const { return expr_result->copy(); }
    template <typename T> T *copy_expr_result() const {
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
    friend inline std::ostream &operator<<(std::ostream &out,
                                           const TOZ3_V2::P4State &state) {
        auto var_map = state.get_state();
        size_t idx = 0;
        // out << "STATIC SCOPE: " << main_scope << "\n";
        for (auto it = var_map.begin(); it != var_map.end(); ++it) {
            out << "SCOPE: " << idx << ": " << *it << "\n";
            idx++;
        }
        return out;
    }
};

} // namespace TOZ3_V2

#endif // _TOZ3_STATE_H_
