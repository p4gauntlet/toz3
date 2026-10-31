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
#include "lib/iterator_range.h"
#include "lib/ordered_map.h"
#include "toz3/common/scope.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/util.h"
#include "type_complex.h"
#include "type_simple.h"
#include "visitor_specialize.h"
#include "z3++.h"

namespace P4::ToZ3 {

bool Z3Visitor::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (const auto &o : p->objects) {
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
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", var->to_string());
        for (const auto &member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, allocate_instance<EnumInstance>(state, state, t, ""_cs, 0), t);
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
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", var->to_string());
        for (const auto &member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        state->add_type(name, t);
        state->declare_var(name, allocate_instance<ErrorInstance>(state, state, t, ""_cs, 0), t);
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
        BUG_CHECK(enum_instance, "Unexpected enum instance %s", var->to_string());
        for (const auto &member : t->members) {
            enum_instance->add_enum_member(member->name.name);
        }
    } else {
        ordered_map<cstring, P4Z3Instance *> input_members;
        const auto member_type = state->resolve_type(t->type);
        state->push_scope();
        for (const auto &member : t->members) {
            visit(member->value);
            auto *value = state->get_expr_result()->cast_allocate(member_type);
            input_members.emplace(member->name.name, value);
            state->declare_var(member->name.name, value, member_type);
        }
        state->pop_scope();
        state->add_type(name, t);
        state->declare_var(
            name, allocate_instance<SerEnumInstance>(state, state, input_members, t, ""_cs, 0), t);
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
    state->declare_var(p->name.name, allocate_instance<ControlInstance>(state, state, p, VarMap{}),
                       p);
    return false;
}

bool Z3Visitor::preorder(const IR::P4Control *c) {
    // Controls can be both a decl and a type
    // TODO: Take a closer look at this...
    state->add_type(c->name.name, c);
    state->declare_var(c->name.name, allocate_instance<ControlInstance>(state, state, c, VarMap{}),
                       c);

    return false;
}

namespace {

void register_callable(P4State *state, const IR::StatOrDecl *node, cstring name,
                       const IR::ParameterList &parameters, bool action = false) {
    size_t required = 0;
    for (const auto &parameter : parameters) {
        if (!parameter->isOptional() && parameter->defaultValue == nullptr &&
            !(action && parameter->direction == IR::Direction::None)) {
            ++required;
        }
    }
    auto *declaration = allocate_instance<P4Declaration>(state, node);
    // Arity narrows the candidates; resolve_callable checks their argument names.
    // Table action references omit control-plane arguments; direct calls supply all.
    for (size_t count = required; count <= parameters.size(); ++count) {
        if (action && count != required && count != parameters.size()) {
            continue;
        }
        state->declare_static_decl(mangle_name(name, count), declaration);
    }
}

}  // namespace

bool Z3Visitor::preorder(const IR::Function *f) {
    register_callable(state, f, f->name, *f->getParameters());
    return false;
}

bool Z3Visitor::preorder(const IR::Method *m) {
    register_callable(state, m, m->name, *m->getParameters());
    return false;
}

bool Z3Visitor::preorder(const IR::P4Action *a) {
    register_callable(state, a, a->name, *a->getParameters(), true);
    return false;
}

bool Z3Visitor::preorder(const IR::P4Table *t) {
    state->declare_static_decl(t->name.name, allocate_instance<P4TableInstance>(state, state, t));
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    auto instance_name = di->name.name;
    auto resolved_type = state->resolve_type(di->type);
    // TODO: Figure out a way to process packages
    if (resolved_type->is<IR::Type_Package>()) {
        state->declare_static_decl(instance_name, allocate_instance<P4Declaration>(state, di));
        return false;
    }
    if (const auto *te = resolved_type->to<IR::Type_Extern>()) {
        const auto specialized = specialize_extern(te, *di->arguments);
        state->declare_var(instance_name,
                           allocate_instance<ExternInstance>(state, state, specialized),
                           specialized);
        return false;
    }
    if (const auto *array = resolved_type->to<IR::Type_Array>()) {
        if (const auto *element = state->resolve_type(array->elementType)->to<IR::Type_Extern>()) {
            auto *specialized = array->clone();
            specialized->elementType = specialize_extern(element, *di->arguments);
            state->declare_var(instance_name, state->gen_instance(instance_name, specialized),
                               specialized);
            return false;
        }
    }
    if (const auto *container = resolved_type->to<IR::IContainer>()) {
        auto var_map = state->merge_args_with_params(this, *di->arguments,
                                                     *container->getConstructorParameters(),
                                                     *container->getTypeParameters());
        resolved_type = resolved_type->apply(TypeModifier(&var_map.types))->checkedTo<IR::Type>();
        state->declare_var(
            instance_name,
            allocate_instance<ControlInstance>(state, state, resolved_type, var_map.values),
            resolved_type);
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
    const auto resolved_type = state->resolve_type(dv->type);
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
    const auto resolved_type = state->resolve_type(pvs->elementType);
    auto pvs_name = infer_name(pvs, pvs->name.name);
    auto *instance = state->gen_instance(pvs_name, resolved_type);
    state->declare_var(pvs->name.name, instance, resolved_type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_MatchKind *dm) {
    for (const auto &member : dm->members) {
        if (state->find_var(member->name.name) != nullptr) {
            continue;
        }
        const auto type = IR::Type_MatchKind::get();
        state->declare_var(
            member->name.name,
            allocate_instance<Z3Bitvector>(
                state, state, type,
                state->get_z3_ctx()->bv_val(state->allocate_match_kind(), INT_WIDTH)),
            type);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::IndexedVector<IR::Declaration> *decls) {
    for (const auto &local_decl : *decls) {
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
        auto int_size = result->get_val()->simplify().get_numeral_int();
        tb->size = int_size;
        tb->expression = nullptr;
    }
}

void DoBitFolding::postorder(IR::Type_Varbits *tb) {
    if (tb->expression != nullptr) {
        tb->expression->apply(Z3Visitor(state, false));
        const auto *result = state->get_expr_result<NumericVal>();
        auto int_size = result->get_val()->simplify().get_numeral_int();
        tb->size = int_size;
        tb->expression = nullptr;
    }
}

void DoBitFolding::postorder(IR::Type_Array *type) {
    if (type->size->is<IR::Constant>()) {
        return;
    }
    type->size->apply(Z3Visitor(state, false));
    const auto value = state->get_expr_result<NumericVal>()->get_val()->simplify();
    type->size = new IR::Constant(big_int{value.get_numeral_uint64()});
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
    if (in_parser && state->find_var("$parser_accepted"_cs) != nullptr) {
        state->update_var("$parser_accepted"_cs,
                          allocate_instance<Z3Bitvector>(state, state, &BOOL_TYPE,
                                                         state->get_z3_ctx()->bool_val(false)));
    }
    // Note the lack of leq in the i > 0 comparison.
    // We do not want to pop the last scope since we use it to get state
    // TODO: There has to be a cleaner way here...
    // Ideally we should track the input/output variables
    for (size_t i = scopes.size(); i > 1; --i) {
        const auto *scope = &scopes.at(i - 1);
        auto copy_out_args = scope->get_copy_out_args();
        std::vector<P4Z3Instance *> copy_out_vals;
        for (const auto &arg_tuple : copy_out_args) {
            auto source = arg_tuple.second;
            auto *val = state->get_var(source);
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

namespace {

// Label matching differs for ordinary values and table actions. Fall-through and
// the implicit default are identical, so collect their branches in one place.
SwitchCasePairs collect_switch_cases(z3::context *ctx, const IR::Vector<IR::SwitchCase> &cases,
                                     const std::function<z3::expr(const IR::Expression *)> &match) {
    SwitchCasePairs branches;
    auto fall_through = ctx->bool_val(false);
    auto matched = ctx->bool_val(false);
    for (const auto &branch : cases) {
        if (branch->label->is<IR::DefaultExpression>()) {
            branches.emplace_back(!matched, branch->statement);
            return branches;
        }
        fall_through = fall_through || match(branch->label);
        if (branch->statement == nullptr) {
            continue;
        }
        branches.emplace_back(fall_through, branch->statement);
        matched = matched || fall_through;
        fall_through = ctx->bool_val(false);
    }
    branches.emplace_back(!matched, nullptr);
    return branches;
}

std::map<cstring, z3::expr> table_action_conditions(Z3Visitor *visitor,
                                                    const P4TableInstance *table) {
    auto *state = visitor->get_state();
    auto *ctx = state->get_z3_ctx();
    std::map<cstring, z3::expr> action_matches;
    if (!table->table_props.immutable) {
        const auto name = table->table_props.table_name + "action_idx";
        const auto action_taken = ctx->int_const(name.c_str());
        int index = 0;
        for (const auto &action : table->table_props.actions) {
            action_matches.emplace(action->method->toString(), action_taken == index && table->hit);
            ++index;
        }
        return action_matches;
    }

    auto evaluated_keys = table->evaluated_keys;
    if (evaluated_keys.empty()) {
        for (const auto &key : table->table_props.keys) {
            visitor->visit(key->expression);
            evaluated_keys.push_back(state->copy_expr_result());
        }
    }
    const auto record = [&](cstring name, const z3::expr &condition) {
        const auto [entry, inserted] = action_matches.emplace(name, condition);
        if (!inserted) {
            entry->second = entry->second || condition;
        }
    };
    auto matched = ctx->bool_val(false);
    for (const auto &entry : table->table_props.entries) {
        const auto match = table->produce_const_match(visitor, &evaluated_keys, entry.first);
        record(entry.second->method->toString(), match && !matched);
        matched = matched || match;
    }
    if (const auto &defaultAction = table->table_props.default_action) {
        record(defaultAction->method->toString(), !matched);
    }
    return action_matches;
}

}  // namespace

bool Z3Visitor::preorder(const IR::SwitchStatement *ss) {
    visit(ss->expression);
    const auto *switch_expr = state->copy_expr_result();
    SwitchCasePairs stmt_vector;

    if (const auto *table = switch_expr->to<P4TableInstance>()) {
        const auto actions = table_action_conditions(this, table);
        stmt_vector =
            collect_switch_cases(state->get_z3_ctx(), ss->cases, [&](const IR::Expression *label) {
                const auto action =
                    actions.find(label->checkedTo<IR::PathExpression>()->toString());
                return action == actions.end() ? state->get_z3_ctx()->bool_val(false)
                                               : action->second;
            });
    } else {
        stmt_vector =
            collect_switch_cases(state->get_z3_ctx(), ss->cases, [&](const IR::Expression *label) {
                visit(label);
                return *switch_expr == *state->get_expr_result();
            });
    }
    // Once this is done we can use our usual ite-execution technique
    // We always add a default statement, so we can set exit/return to true
    BUG_CHECK(!stmt_vector.empty(), "Statement vector can not be empty.");
    bool has_exited = true;
    bool has_returned = true;
    const auto loop_stopped = !loops.empty() && loops.back().stopped;
    bool all_stopped = true;
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    for (auto &stmt : stmt_vector) {
        auto case_match = stmt.first.simplify();
        if (case_match.is_false()) {
            continue;
        }
        if (!loops.empty()) {
            loops.back().stopped = loop_stopped;
        }
        const auto *case_stmt = stmt.second;
        auto old_vars = state->clone_vars();
        state->push_forward_cond(case_match);
        visit(case_stmt);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (!(call_has_exited || stmt_has_returned)) {
            case_states.emplace_back(case_match, state->clone_vars());
        }
        all_stopped = all_stopped && !loops.empty() && loops.back().stopped;
        has_exited = has_exited && call_has_exited;
        has_returned = has_returned && stmt_has_returned;
        state->set_exit(false);
        state->set_returned(false);
        state->restore_vars(old_vars);
    }
    state->set_exit(has_exited);
    state->set_returned(has_returned);
    if (!loops.empty()) {
        loops.back().stopped = all_stopped;
    }

    for (auto &caseState : Util::iterator_range(case_states).reverse()) {
        state->merge_vars(caseState.first, caseState.second);
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
    const auto loop_stopped = !loops.empty() && loops.back().stopped;
    state->push_forward_cond(z3_cond);
    visit(ifs->ifTrue);
    state->pop_forward_cond();
    auto then_has_exited = state->has_exited();
    auto then_has_returned = state->has_returned();
    const auto then_stopped = !loops.empty() && loops.back().stopped;
    VarMap then_vars;
    if (then_has_exited || then_has_returned) {
        then_vars = old_vars;
    } else {
        then_vars = state->clone_vars();
    }
    state->set_exit(false);
    state->set_returned(false);
    if (!loops.empty()) {
        loops.back().stopped = loop_stopped;
    }

    state->restore_vars(old_vars);
    auto old_state = state->clone_vars();
    state->push_forward_cond(!z3_cond);
    visit(ifs->ifFalse);
    state->pop_forward_cond();
    auto else_has_exited = state->has_exited();
    auto else_has_returned = state->has_returned();
    if (!loops.empty()) {
        loops.back().stopped = then_stopped && loops.back().stopped;
    }
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
    for (const auto &c : b->components) {
        auto guard = state->get_z3_ctx()->bool_val(true);
        if (!loops.empty()) {
            guard = !(loops.back().break_condition || loops.back().continue_condition);
            guard = guard.simplify();
        }
        if (guard.is_false()) {
            break;
        }
        if (guard.is_true()) {
            visit(c);
        } else {
            const auto skipped = state->clone_vars();
            state->push_forward_cond(guard);
            visit(c);
            state->pop_forward_cond();
            state->merge_vars(!guard, skipped);
        }
        if (state->has_returned() || state->has_exited()) {
            break;
        }
        if (!loops.empty() && loops.back().stopped) {
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

bool Z3Visitor::preorder(const IR::OpAssignmentStatement *as) {
    const auto target = get_member_struct(state, this, as->left);
    const auto *left = get_member(state, target)->copy();
    visit(as->right);
    const auto *right = state->get_expr_result();
    P4Z3Instance *result = nullptr;
    if (as->is<IR::MulAssign>()) {
        result = *left * *right;
    } else if (as->is<IR::DivAssign>()) {
        result = *left / *right;
    } else if (as->is<IR::ModAssign>()) {
        result = *left % *right;
    } else if (as->is<IR::AddAssign>()) {
        result = *left + *right;
    } else if (as->is<IR::SubAssign>()) {
        result = *left - *right;
    } else if (as->is<IR::AddSatAssign>()) {
        result = left->operatorAddSat(*right);
    } else if (as->is<IR::SubSatAssign>()) {
        result = left->operatorSubSat(*right);
    } else if (as->is<IR::ShlAssign>()) {
        result = *left << *right;
    } else if (as->is<IR::ShrAssign>()) {
        result = *left >> *right;
    } else if (as->is<IR::BAndAssign>()) {
        result = *left & *right;
    } else if (as->is<IR::BOrAssign>()) {
        result = *left | *right;
    } else if (as->is<IR::BXorAssign>()) {
        result = *left ^ *right;
    } else {
        P4C_UNIMPLEMENTED("Compound assignment %s not implemented", as);
    }
    state->set_var(target, result);
    return false;
}

}  // namespace P4::ToZ3
