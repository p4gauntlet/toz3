#include "visitor_interpret.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ir/id.h"
#include "ir/vector.h"
#include "lib/cstring.h"
#include "lib/ordered_map.h"
#include "toz3/common/scope.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/util.h"
#include "type_complex.h"
#include "type_simple.h"
#include "z3++.h"

namespace P4::ToZ3 {

bool Z3Visitor::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (const auto *o : p->objects) {
        visit(o);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Type_StructLike *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_StructLike>();
    state->add_type(t->name.name, state->resolve_type(t));
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Enum *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Enum>();
    // TODO: Enums are really nasty because we also need to access them
    // TODO: Simplify this.
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, new EnumInstance(state, t, ""_cs, 0), t);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Error *t) {
    // TODO: Simplify this.
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Error>();
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, new ErrorInstance(state, t, ""_cs, 0), t);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Type_SerEnum *t) {
    // TODO: Enums are really nasty because we also need to access them
    // TODO: Simplify this.
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_SerEnum>();
    auto name = t->name.name;
    auto *var = state->find_var(name);
    // Every P4 program is initialized with an error namespace
    // according to the spec
    // So if the error exists, we merge
    if (var != nullptr) {
        auto *enum_instance = var->to_mut<EnumBase>();
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", enum_instance->to_string());
        for (const auto *member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        ordered_map<cstring, P4Z3Instance *> input_members;
        const auto *member_type = state->resolve_type(t->type);
        for (const auto *member : t->members) {
            visit(member->value);
            input_members.emplace(member->name.name,
                                  state->get_expr_result()->cast_allocate(member_type));
        }
        state->add_type(name, t);
        state->declare_var(name, new SerEnumInstance(state, input_members, t, ""_cs, 0), t);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Extern *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Extern>();
    state->add_type(t->name.name, t);

    return false;
}

bool Z3Visitor::preorder(const IR::Type_Typedef *t) {
    const auto *type_clone = t->type->apply(DoBitFolding(state))->checkedTo<IR::Type>();
    state->add_type(t->name.name, state->resolve_type(type_clone));
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Newtype *t) {
    const auto *type_clone = t->type->apply(DoBitFolding(state))->checkedTo<IR::Type>();
    state->add_type(t->name.name, state->resolve_type(type_clone));
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Package *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Package>();
    state->add_type(t->name.name, state->resolve_type(t));
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Parser *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Parser>();
    state->add_type(t->name.name, state->resolve_type(t));
    return false;
}

bool Z3Visitor::preorder(const IR::Type_Control *t) {
    t = t->apply(DoBitFolding(state))->checkedTo<IR::Type_Control>();
    state->add_type(t->name.name, state->resolve_type(t));
    return false;
}

bool Z3Visitor::preorder(const IR::P4Parser *p) {
    // Parsers can be both a var and a type
    // TODO: Take a closer look at this...
    state->add_type(p->name.name, p);
    state->declare_var(p->name.name, new ControlInstance(state, p, {}), p);
    return false;
}

bool Z3Visitor::preorder(const IR::P4Control *c) {
    // Controls can be both a decl and a type
    // TODO: Take a closer look at this...
    state->add_type(c->name.name, c);
    state->declare_var(c->name.name, new ControlInstance(state, c, {}), c);

    return false;
}

bool Z3Visitor::preorder(const IR::Function *f) {
    // TODO: Overloading uses num of parameters, it should use types
    cstring overloaded_name = f->name.name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : f->getParameters()->parameters) {
        if (param->isOptional() || param->defaultValue != nullptr) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto *decl = new P4Declaration(f);
    for (auto idx = 0; idx <= num_optional_params; ++idx) {
        // The IR has bizarre side effects when storing pointers in a map
        // TODO: Think about how to simplify this, maybe use their vector
        auto name = overloaded_name + std::to_string(num_params + idx);
        state->declare_static_decl(name, decl);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Method *m) {
    // TODO: Overloading uses num of parameters, it should use types
    cstring overloaded_name = m->name.name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : m->getParameters()->parameters) {
        if (param->isOptional() || param->defaultValue != nullptr) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto *decl = new P4Declaration(m);
    for (auto idx = 0; idx <= num_optional_params; ++idx) {
        // The IR has bizarre side effects when storing pointers in a map
        // TODO: Think about how to simplify this, maybe use their vector
        auto name = overloaded_name + std::to_string(num_params + idx);
        state->declare_static_decl(name, decl);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::P4Action *a) {
    // TODO: Overloading uses num of parameters, it should use types
    cstring overloaded_name = a->name.name;
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : a->getParameters()->parameters) {
        if (param->direction == IR::Direction::None || param->isOptional() ||
            param->defaultValue != nullptr) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    auto *decl = new P4Declaration(a);
    cstring name_basic = overloaded_name + std::to_string(num_params);
    state->declare_static_decl(name_basic, decl);
    // The IR has bizarre side effects when storing pointers in a map
    // TODO: Think about how to simplify this, maybe use their vector
    if (num_optional_params != 0) {
        cstring name_opt = overloaded_name + std::to_string(num_params + num_optional_params);
        state->declare_static_decl(name_opt, decl);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::P4Table *t) {
    state->declare_static_decl(t->name.name, new P4TableInstance(state, t));
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    auto instance_name = di->name.name;
    const IR::Type *resolved_type = state->resolve_type(di->type);
    // TODO: Figure out a way to process packages
    if (resolved_type->is<IR::Type_Package>()) {
        state->declare_static_decl(instance_name, new P4Declaration(di));
        return false;
    }
    if (const auto *te = resolved_type->to<IR::Type_Extern>()) {
        // TODO: Clean this mess up.
        // const auto *ext_const = te->lookupConstructor(di->arguments);
        // const IR::ParameterList *params = nullptr;
        // params = ext_const->getParameters();
        state->declare_var(instance_name, new ExternInstance(state, te), te);
        return false;
    }
    if (const auto *ctrl_decl = resolved_type->to<IR::Type_Declaration>()) {
        const IR::ParameterList *params = nullptr;
        const IR::TypeParameters *type_params = nullptr;
        if (const auto *c = ctrl_decl->to<IR::P4Control>()) {
            params = c->getConstructorParameters();
            type_params = c->getTypeParameters();
        } else if (const auto *p = ctrl_decl->to<IR::P4Parser>()) {
            params = p->getConstructorParameters();
            type_params = p->getTypeParameters();
        } else {
            P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.", ctrl_decl,
                              ctrl_decl->node_type_name());
        }
        auto var_map = state->merge_args_with_params(this, *di->arguments, *params, *type_params);
        state->declare_var(instance_name, new ControlInstance(state, ctrl_decl, var_map.second),
                           ctrl_decl);
        return false;
    }
    P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ", resolved_type,
                      resolved_type->node_type_name());
}

bool Z3Visitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left = nullptr;
    const auto *type_clone = dc->type->apply(DoBitFolding(state))->checkedTo<IR::Type>();
    const auto *resolved_type = state->resolve_type(type_clone);
    if (dc->initializer != nullptr) {
        visit(dc->initializer);
        left = state->get_expr_result()->cast_allocate(resolved_type);
    } else {
        left = state->gen_instance(cstring(UNDEF_LABEL), resolved_type);
    }
    state->declare_var(dc->name.name, left, resolved_type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left = nullptr;
    const auto *resolved_type = state->resolve_type(dv->type);
    if (dv->initializer != nullptr) {
        visit(dv->initializer);
        left = state->get_expr_result()->cast_allocate(resolved_type);
    } else {
        left = state->gen_instance(cstring(UNDEF_LABEL), resolved_type);
    }
    state->declare_var(dv->name.name, left, resolved_type);

    return false;
}

bool Z3Visitor::preorder(const IR::P4ValueSet *pvs) {
    const auto *resolved_type = state->resolve_type(pvs->elementType);
    auto pvs_name = infer_name(pvs->getAnnotations(), pvs->name.name);
    auto *instance = state->gen_instance(pvs_name, resolved_type);
    state->declare_var(pvs->name.name, instance, resolved_type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_MatchKind * /*dm */) {
    // TODO: Figure out purpose of Declaration_MatchKind
    // state->add_decl(dm->name.name, dm);
    return false;
}

bool Z3Visitor::preorder(const IR::IndexedVector<IR::Declaration> *decls) {
    for (const auto *local_decl : *decls) {
        visit(local_decl);
    }
    return false;
}

void DoBitFolding::postorder(IR::Type_Bits *tb) {
    // We need to resolve any bits that have an expression as size
    // We assume that the result is a constant, otherwise this will fail
    if (tb->expression != nullptr) {
        tb->expression->apply(Z3Visitor(state, false));
        const auto *result = state->get_expr_result<NumericVal>();
        auto int_size = result->get_val()->simplify().get_numeral_uint64();
        tb->size = int_size;
        tb->expression = nullptr;
    }
}

void DoBitFolding::postorder(IR::Type_Varbits *tb) {
    if (tb->expression != nullptr) {
        tb->expression->apply(Z3Visitor(state, false));
        const auto *result = state->get_expr_result<NumericVal>();
        auto int_size = result->get_val()->simplify().get_numeral_uint64();
        tb->size = int_size;
        tb->expression = nullptr;
    }
}

/***
===============================================================================
EmptyStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::EmptyStatement * /*e*/) { return false; }

/***
===============================================================================
ReturnStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::ReturnStatement *r) {
    auto forward_conds = state->get_forward_conds();
    auto return_conds = state->get_return_conds();
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (const auto &sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    for (const auto &sub_cond : return_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();
    // If we do not even return do not bother with collecting results.
    if (r->expression != nullptr) {
        visit(r->expression);
        state->push_return_expr(cond && exit_cond, state->copy_expr_result());
    }
    state->push_return_state(cond && exit_cond, state->clone_vars());
    state->push_return_cond(!cond);
    state->set_returned(true);

    return false;
}

/***
===============================================================================
ExitStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::ExitStatement * /*e*/) {
    auto forward_conds = state->get_forward_conds();
    auto return_conds = state->get_return_conds();
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (z3::expr &sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    for (z3::expr &sub_cond : return_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();

    auto scopes = state->get_state();
    auto old_state = state->clone_state();
    // Note the lack of leq in the i > 0 comparison.
    // We do not want to pop the last scope since we use it to get state
    // TODO: There has to be a cleaner way here...
    // Ideally we should track the input/output variables
    for (int64_t i = scopes.size() - 1; i > 0; --i) {
        const auto *scope = &scopes.at(i);
        auto copy_out_args = scope->get_copy_out_args();
        std::vector<P4Z3Instance *> copy_out_vals;
        for (const auto &arg_tuple : copy_out_args) {
            auto source = arg_tuple.second;
            auto *val = state->get_var(source);
            // Exit in parsers means that everything is invalid
            if (in_parser) {
                if (auto *si = val->to_mut<StructBase>()) {
                    auto invalid_bool = state->get_z3_ctx()->bool_val(false);
                    si->propagate_validity(&invalid_bool);
                }
            }
            copy_out_vals.push_back(val);
        }

        state->pop_scope();
        size_t idx = 0;
        for (auto &arg_tuple : copy_out_args) {
            auto target = arg_tuple.first;
            state->set_var(target, copy_out_vals[idx]);
            idx++;
        }
    }
    auto exit_vars = state->clone_vars();
    state->restore_state(old_state);
    state->add_exit_state(exit_cond && cond, exit_vars);
    state->set_exit_cond(exit_cond && !cond);
    state->set_exit(true);

    return false;
}

/***
===============================================================================
SwitchStatement
===============================================================================
***/

using SwitchCasePairs = std::vector<std::pair<z3::expr, const IR::Statement *>>;

SwitchCasePairs handle_immutable_table_switch(Z3Visitor *visitor, const P4TableInstance *table,
                                              const IR::Vector<IR::SwitchCase> &cases) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    SwitchCasePairs stmt_vector;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    bool has_default = false;
    std::vector<const P4Z3Instance *> evaluated_keys;
    for (const auto *key : table->table_props.keys) {
        // TODO: This should not be necessary
        // We have this information already
        visitor->visit(key->expression);
        const auto *key_eval = state->copy_expr_result();
        evaluated_keys.push_back(key_eval);
    }
    auto new_entries = table->table_props.entries;
    for (const auto *switch_case : cases) {
        if (const auto *label = switch_case->label->to<IR::PathExpression>()) {
            z3::expr cond = ctx->bool_val(false);
            for (auto it = new_entries.begin(); it != new_entries.end();) {
                auto entry = *it;
                const auto *keys = entry.first;
                const auto *action = entry.second;
                if (label->toString() != action->method->toString()) {
                    ++it;
                    continue;
                }
                cond = cond || table->produce_const_match(visitor, &evaluated_keys, keys);
                it = new_entries.erase(it);
            }
            // There is no block for the switch.
            // This expressions falls through to the next switch case.
            fall_through = fall_through || cond;
            if (switch_case->statement == nullptr) {
                continue;
            }
            auto case_match = fall_through;
            // If the entries are empty we exhausted all possible matches
            // TODO: Not sure if this is a good idea?
            if (new_entries.empty()) {
                case_match = ctx->bool_val(true);
            }
            // Matches the condition OR all the other fall-through switches
            fall_through = ctx->bool_val(false);
            matches = matches || case_match;
            stmt_vector.emplace_back(case_match, switch_case->statement);
        } else if (switch_case->label->is<IR::DefaultExpression>()) {
            has_default = true;
            stmt_vector.emplace_back(!matches, switch_case->statement);
        } else {
            P4C_UNIMPLEMENTED("Case expression %s of type %s not supported.", switch_case->label,
                              switch_case->label->node_type_name());
        }
    }
    // If we did not encounter a default statement, implicitly add it
    if (!has_default) {
        stmt_vector.emplace_back(!matches, nullptr);
    }
    return stmt_vector;
}

SwitchCasePairs handle_table_switch(Z3Visitor *visitor, const P4TableInstance *table,
                                    const IR::Vector<IR::SwitchCase> &cases) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    SwitchCasePairs stmt_vector;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    bool has_default = false;
    auto table_action_name = table->table_props.table_name + "action_idx";
    auto action_taken = ctx->int_const(table_action_name.c_str());
    std::map<cstring, int> action_mapping;
    size_t idx = 0;
    for (const auto *action : table->table_props.actions) {
        const auto *method_expr = action->method;
        const auto *path = method_expr->checkedTo<IR::PathExpression>();
        action_mapping[path->path->name.name] = idx;
        idx++;
    }
    // now actually map all the statements together
    for (const auto *switch_case : cases) {
        if (const auto *label = switch_case->label->to<IR::PathExpression>()) {
            auto mapped_idx = action_mapping[label->path->name.name];
            auto cond = action_taken == mapped_idx;
            // There is no block for the switch.
            // This expressions falls through to the next switch case.
            fall_through = fall_through || cond;
            if (switch_case->statement == nullptr) {
                continue;
            }
            auto case_match = fall_through;
            // Matches the condition OR all the other fall-through switches
            fall_through = ctx->bool_val(false);
            matches = matches || case_match;
            stmt_vector.emplace_back(case_match, switch_case->statement);
        } else if (switch_case->label->is<IR::DefaultExpression>()) {
            has_default = true;
            stmt_vector.emplace_back(!matches, switch_case->statement);
            break;
        } else {
            P4C_UNIMPLEMENTED("Case expression %s of type %s not supported.", switch_case->label,
                              switch_case->label->node_type_name());
        }
    }
    // If we did not encounter a default statement, implicitly add it
    if (!has_default) {
        stmt_vector.emplace_back(!matches, nullptr);
    }
    return stmt_vector;
}

SwitchCasePairs collect_stmt_vec_expr(Z3Visitor *visitor, const P4Z3Instance *switch_expr,
                                      const IR::Vector<IR::SwitchCase> &cases) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    SwitchCasePairs stmt_vector;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    bool has_default = false;
    for (const auto *switch_case : cases) {
        z3::expr cond = ctx->bool_val(true);
        if (switch_case->label->is<IR::DefaultExpression>()) {
            has_default = true;
            stmt_vector.emplace_back(!matches, switch_case->statement);
            break;
        }
        visitor->visit(switch_case->label);
        const auto *matched_expr = state->get_expr_result();
        cond = *switch_expr == *matched_expr;
        // There is no block for the switch.
        // This expressions falls through to the next switch case.
        fall_through = fall_through || cond;
        if (switch_case->statement == nullptr) {
            continue;
        }
        auto case_match = fall_through;
        // Matches the condition OR all the other fall-through switches
        fall_through = ctx->bool_val(false);
        matches = matches || case_match;
        stmt_vector.emplace_back(case_match, switch_case->statement);
    }
    // If we did not encounter a default statement, implicitly add it
    if (!has_default) {
        stmt_vector.emplace_back(!matches, nullptr);
    }
    return stmt_vector;
}

bool Z3Visitor::preorder(const IR::SwitchStatement *ss) {
    visit(ss->expression);
    const auto *switch_expr = state->get_expr_result();
    SwitchCasePairs stmt_vector;

    // First map the individual statement blocks to their respective matches
    // Tables are a little complicated so we have to take special care
    if (const auto *table = switch_expr->to<P4TableInstance>()) {
        if (table->table_props.immutable) {
            stmt_vector = handle_immutable_table_switch(this, table, ss->cases);
        } else {
            stmt_vector = handle_table_switch(this, table, ss->cases);
        }
    } else {
        stmt_vector = collect_stmt_vec_expr(this, switch_expr->copy(), ss->cases);
    }
    // Once this is done we can use our usual ite-execution technique
    // We always add a default statement, so we can set exit/return to true
    BUG_CHECK(!stmt_vector.empty(), "Statement vector can not be empty.");
    bool has_exited = true;
    bool has_returned = true;
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    for (auto &stmt : stmt_vector) {
        auto case_match = stmt.first;
        const auto *case_stmt = stmt.second;
        auto old_vars = state->clone_vars();
        state->push_forward_cond(case_match);
        visit(case_stmt);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (!(call_has_exited || stmt_has_returned)) {
            case_states.emplace_back(case_match, state->get_vars());
        }
        has_exited = has_exited && call_has_exited;
        has_returned = has_returned && stmt_has_returned;
        state->set_exit(false);
        state->set_returned(false);
        state->restore_vars(old_vars);
    }
    state->set_exit(has_exited);
    state->set_returned(has_returned);

    for (auto it = case_states.rbegin(); it != case_states.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    return false;
}

/***
===============================================================================
IfStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto z3_cond = state->get_expr_result<Z3Bitvector>()->get_val()->simplify();
    if (z3_cond.is_true()) {
        visit(ifs->ifTrue);
        return false;
    }
    if (z3_cond.is_false()) {
        visit(ifs->ifFalse);
        return false;
    }
    auto old_vars = state->clone_vars();
    state->push_forward_cond(z3_cond);
    visit(ifs->ifTrue);
    state->pop_forward_cond();
    auto then_has_exited = state->has_exited();
    auto then_has_returned = state->has_returned();
    VarMap then_vars;
    if (then_has_exited || then_has_returned) {
        then_vars = old_vars;
    } else {
        then_vars = state->clone_vars();
    }
    state->set_exit(false);
    state->set_returned(false);

    state->restore_vars(old_vars);
    auto old_state = state->clone_vars();
    state->push_forward_cond(!z3_cond);
    visit(ifs->ifFalse);
    state->pop_forward_cond();
    auto else_has_exited = state->has_exited();
    auto else_has_returned = state->has_returned();
    if (else_has_exited || else_has_returned) {
        state->restore_vars(old_state);
    }

    // If both branches have returned we set the if statement to returned or
    // exited
    state->set_exit(then_has_exited && else_has_exited);
    state->set_returned(then_has_returned && else_has_returned);
    state->merge_vars(z3_cond, then_vars);
    return false;
}

/***
===============================================================================
BlockStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (const auto *c : b->components) {
        visit(c);
        if (state->has_returned() || state->has_exited()) {
            break;
        }
    }
    return false;
}

/***
===============================================================================
MethodCallStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::MethodCallStatement *mcs) {
    visit(mcs->methodCall);
    return false;
}

/***
===============================================================================
AssignmentStatement
===============================================================================
***/

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    state->set_var(this, as->left, as->right);
    return false;
}

}  // namespace P4::ToZ3
