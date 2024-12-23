#ifndef TOZ3_COMMON_STATE_H_
#define TOZ3_COMMON_STATE_H_

#include <z3++.h>

#include <cstdint>
#include <cstdio>
#include <ostream>
#include <set>
#include <typeinfo>
#include <utility>
#include <vector>

#include "ir/ir.h"
#include "ir/vector.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/exceptions.h"
#include "scope.h"
#include "toz3/common/type_base.h"
#include "toz3/common/type_complex.h"
#include "toz3/common/type_simple.h"

namespace P4::ToZ3 {

MemberStruct get_member_struct(P4State *state, Visitor *visitor, const IR::Expression *target);
std::vector<std::pair<z3::expr, P4Z3Instance *>> get_hdr_pairs(P4State *state,
                                                               const MemberStruct &member_struct);

class P4State {
 private:
    ProgState scopes;
    P4Scope main_scope;
    z3::context *ctx;
    P4Z3Instance *expr_result = nullptr;
    // Exit vars
    bool is_exited = false;
    std::vector<std::pair<z3::expr, VarMap>> exit_states;
    z3::expr exit_cond = ctx->bool_val(true);
    P4Scope *get_mut_current_scope() { return &scopes.back(); }
    void set_var(Visitor *visitor, const IR::Expression *target, P4Z3Instance *rval);
    P4Declaration *find_static_decl(cstring name, P4Scope **owner_scope);
    P4Z3Instance *find_var(cstring name, P4Scope **owner_scope);
    const IR::Type *find_type(cstring type_name, P4Scope **owner_scope);

 public:
    const P4Scope &get_current_scope() const { return scopes.back(); }
    bool has_exited() const { return is_exited; }
    void set_exit(bool exit_state) { is_exited = exit_state; }

    explicit P4State(z3::context *context) : ctx(context) {
        // These two labels are part of the built in declarations.
        // We only need to add them once.
        declare_static_decl(IR::ParserState::accept,
                            new P4Declaration(new IR::ReturnStatement(nullptr)));
        declare_static_decl(IR::ParserState::reject, new P4Declaration(new IR::ExitStatement()));
    }

    /****** GETTERS ******/
    ProgState get_state() const { return scopes; }
    z3::context *get_z3_ctx() const { return ctx; }
    const P4Z3Instance *get_expr_result() const { return expr_result; }
    template <typename T>
    const T *get_expr_result() const {
        if (auto cast_result = expr_result->to<T>()) {
            return cast_result;
        }
        BUG("Could not cast to type %s.", typeid(T).name());
    }
    /****** ALLOCATIONS ******/
    z3::expr gen_z3_expr(cstring name, const IR::Type *type);
    P4Z3Instance *gen_instance(cstring name, const IR::Type *type, uint64_t id = 0);

    /****** COPY-IN/COPY-OUT ******/
    std::pair<CopyArgs, VarMap> merge_args_with_params(Visitor *visitor,
                                                       const IR::Vector<IR::Argument> &args,
                                                       const IR::ParameterList &params,
                                                       const IR::TypeParameters &type_params);
    void copy_in(Visitor *visitor, const ParamInfo &param_info);
    void copy_out();
    void set_copy_out_args(const CopyArgs &out_args) {
        auto *scope = get_mut_current_scope();
        scope->set_copy_out_args(out_args);
    }
    CopyArgs get_copy_out_args() const {
        auto scope = get_current_scope();
        return scope.get_copy_out_args();
    }
    /****** PARSER STATES ******/
    void add_visited_state(cstring state_name) {
        auto *scope = get_mut_current_scope();
        scope->add_visited_state(state_name);
    }
    void clear_visited_states() {
        auto *scope = get_mut_current_scope();
        scope->clear_visited_states();
    }
    std::set<cstring> get_visited_states() {
        auto *scope = get_mut_current_scope();
        return scope->get_visited_states();
    }
    void set_visited_states(const std::set<cstring> &new_states) {
        auto *scope = get_mut_current_scope();
        scope->set_visited_states(new_states);
    }
    bool state_is_visited(cstring state_name) {
        auto *scope = get_mut_current_scope();
        return scope->state_is_visited(state_name);
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
    void merge_vars(const z3::expr &cond, const VarMap &then_map) const;
    z3::expr get_exit_cond() const { return exit_cond; }
    void set_exit_cond(const z3::expr &forward_cond) { exit_cond = forward_cond; }
    void clear_exit_state() { exit_states.clear(); }
    void merge_exit_states() {
        // Merge the exit states
        for (const auto &exit_tuple : exit_states) {
            merge_vars(exit_tuple.first, exit_tuple.second);
        }
        // Clear the exit states
        exit_states.clear();
        exit_cond = ctx->bool_val(true);
        is_exited = false;
    }
    void add_exit_state(const z3::expr &cond, const VarMap &exit_state) {
        exit_states.emplace_back(cond, exit_state);
    }
    std::vector<z3::expr> get_forward_conds() const {
        std::vector<z3::expr> forward_conds;
        for (const auto &scope : scopes) {
            auto sub_conds = scope.get_forward_conds();
            forward_conds.insert(forward_conds.end(), sub_conds.begin(), sub_conds.end());
        }
        return forward_conds;
    }
    void push_forward_cond(const z3::expr &forward_cond) {
        auto *scope = get_mut_current_scope();
        scope->push_forward_cond(forward_cond);
    }
    std::vector<z3::expr> get_return_conds() const {
        std::vector<z3::expr> return_conds;
        for (const auto &scope : scopes) {
            auto sub_conds = scope.get_return_conds();
            return_conds.insert(return_conds.end(), sub_conds.begin(), sub_conds.end());
        }
        return return_conds;
    }
    void push_return_cond(const z3::expr &return_cond) {
        get_mut_current_scope()->push_return_cond(return_cond);
    }
    void pop_forward_cond() {
        auto *scope = get_mut_current_scope();
        scope->pop_forward_cond();
    }
    bool has_returned() const { return get_current_scope().has_returned(); }
    void set_returned(bool return_state) { get_mut_current_scope()->set_returned(return_state); }
    void push_return_expr(const z3::expr &cond, P4Z3Instance *return_expr) {
        return get_mut_current_scope()->push_return_expr(cond, return_expr);
    }
    std::vector<std::pair<z3::expr, P4Z3Instance *>> get_return_exprs() {
        return get_mut_current_scope()->get_return_exprs();
    }
    void push_return_state(const z3::expr &cond, const VarMap &state) {
        return get_mut_current_scope()->push_return_state(cond, state);
    }
    std::vector<std::pair<z3::expr, VarMap>> get_return_states() const {
        return get_current_scope().get_return_states();
    }

    /****** TYPES ******/
    const IR::Type *resolve_type(const IR::Type *type) const;
    void add_type(cstring type_name, const IR::Type *t);
    const IR::Type *get_type(cstring type_name) const;
    const IR::Type *check_for_type(cstring type_name) const;
    const IR::Type *check_for_type(const IR::Type *t) const;

    /****** VARIABLES ******/
    P4Z3Instance *find_var(cstring name) const;
    void update_var(cstring name, P4Z3Instance *var);
    void declare_var(cstring name, P4Z3Instance *var, const IR::Type *decl_type);
    P4Z3Instance *get_var(cstring name) const;
    template <typename T>
    const T *get_var(cstring name) const {
        const auto *var = get_var(name);
        return var->to<T>();
    }
    const IR::Type *get_var_type(cstring decl_name) const;
    void set_var(Visitor *visitor, const IR::Expression *target, const IR::Expression *rval);
    void set_var(const MemberStruct &member_struct, P4Z3Instance *rval);

    /****** DECLARATIONS ******/
    void declare_static_decl(cstring name, P4Declaration *decl);
    const P4Declaration *get_static_decl(cstring name) const;
    P4Declaration *find_static_decl(cstring name) const;
    template <typename T>
    const T *get_static_decl(cstring name) const {
        const auto *decl = get_static_decl(name);
        return decl->to<T>();
    }
    /****** EXPRESSION RESULTS ******/
    P4Z3Instance *copy_expr_result() const { return expr_result->copy(); }
    template <typename T>
    T *copy_expr_result() const {
        auto *intermediate = expr_result->copy();
        if (auto cast_result = intermediate->to_mut<T>()) {
            return cast_result;
        }
        BUG("Could not cast to type %s.", typeid(T).name());
    }
    void set_expr_result(P4Z3Instance *result) { expr_result = result; }
    friend inline std::ostream &operator<<(std::ostream &out, const P4State &state) {
        auto var_map = state.get_state();
        size_t idx = 0;
        // out << "STATIC SCOPE: " << main_scope << "\n";
        for (const auto &var : var_map) {
            out << "SCOPE " << idx << ": " << var << "\n";
            idx++;
        }
        return out;
    }
};

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_STATE_H_
