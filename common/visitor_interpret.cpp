#include <cstdio>
#include <utility>

#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/log.h"
#include "type_complex.h"
#include "type_simple.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3 {

bool Z3Visitor::preorder(const IR::P4Control *c) {
    TypeVisitor map_builder = TypeVisitor(state);

    for (const IR::Declaration *local_decl : c->controlLocals) {
        local_decl->apply(map_builder);
    }
    visit(c->body);
    return false;
}

bool Z3Visitor::preorder(const IR::P4Parser *) {
    // TODO: Implement
    return false;
}

bool Z3Visitor::preorder(const IR::P4Action *a) {
    visit(a->body);
    state->set_expr_result(new VoidResult());
    return false;
}

bool Z3Visitor::preorder(const IR::Function *f) {
    visit(f->body);

    // We start with the last return expression, which is the final return.
    // The final return may not have a condition, so this is a good fit.
    auto return_exprs = state->get_return_exprs();
    auto begin = return_exprs.rbegin();
    auto end = return_exprs.rend();
    if (begin != end) {
        const auto *return_type = f->type->returnType;
        auto *merged_return = begin->second->cast_allocate(return_type);
        for (auto it = std::next(begin); it != end; ++it) {
            z3::expr cond = it->first;
            const auto *then_var = it->second->cast_allocate(return_type);
            merged_return->merge(cond, *then_var);
        }
        state->set_expr_result(merged_return);
    } else {
        // if there are no return expression return a void result
        state->set_expr_result(new VoidResult());
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Method *m) {
    auto method_name = infer_name(m->getAnnotations(), m->name.name);
    const auto *method_type = m->type->returnType;
    // TODO: Different types of arguments and multiple calls
    for (const auto *param : *m->getParameters()) {
        cstring param_name = param->name.name;
        cstring merged_param_name = method_name + "_" + param_name;
        if (param->direction == IR::Direction::Out ||
            param->direction == IR::Direction::InOut) {
            auto *instance = state->gen_instance(merged_param_name, param->type,
                                                 0, method_name);
            state->update_var(param_name, instance);
        }
    }
    state->set_expr_result(state->gen_instance(method_name, method_type));
    return false;
}

void process_table_properties(
    const IR::P4Table *p4t, std::vector<const IR::KeyElement *> *keys,
    std::vector<const IR::MethodCallExpression *> *actions,
    const IR::MethodCallExpression **default_action, bool *immutable) {

    for (const auto *p : p4t->properties->properties) {
        const auto *val = p->value;
        if (const auto *key = val->to<IR::Key>()) {
            for (const auto *ke : key->keyElements) {
                keys->push_back(ke);
            }
        } else if (const auto *action_list = val->to<IR::ActionList>()) {
            for (const auto *act : action_list->actionList) {
                bool ignore_default = false;
                for (const auto *anno : act->getAnnotations()->annotations) {
                    if (anno->name.name == "defaultonly") {
                        ignore_default = true;
                    }
                }
                if (ignore_default) {
                    continue;
                }
                if (const auto *method_call =
                        act->expression->to<IR::MethodCallExpression>()) {
                    actions->push_back(method_call);
                } else if (act->expression->is<IR::PathExpression>()) {
                    actions->push_back(
                        new IR::MethodCallExpression(act->expression));
                } else {
                    P4C_UNIMPLEMENTED("Unsupported action entry %s of type %s",
                                      act, act->expression->node_type_name());
                }
            }
        } else if (const auto *expr_val = val->to<IR::ExpressionValue>()) {
            // resolve a default action
            if (p->name.name == "default_action") {
                if (const auto *method_call =
                        expr_val->expression->to<IR::MethodCallExpression>()) {
                    *default_action = method_call;
                } else if (expr_val->expression->is<IR::PathExpression>()) {
                    *default_action =
                        new IR::MethodCallExpression(expr_val->expression);
                } else {
                    P4C_UNIMPLEMENTED(
                        "Unsupported expression value %s of type %s", expr_val,
                        expr_val->expression->node_type_name());
                }
            } else {
                warning("ExpressionValue property %s of type %s\n",
                        expr_val->expression->toString().c_str(),
                        expr_val->expression->node_type_name().c_str());
            }
        } else {
            warning("Unknown property %s of type %s\n", p->toString().c_str(),
                    p->value->node_type_name().c_str());
        }

        // if the entries properties is constant it means the entries are fixed
        // we cannot add or remove table entries
        if (p->name.name == "entries" && p->isConstant) {
            *immutable = true;
        }
    }
}

z3::expr compute_table_hit(Z3Visitor *visitor, cstring table_name,
                           const std::vector<const IR::KeyElement *> &keys) {
    const auto *state = visitor->state;
    auto *ctx = state->get_z3_ctx();
    z3::expr hit = ctx->bool_val(false);
    for (std::size_t idx = 0; idx < keys.size(); ++idx) {
        const auto *key = keys.at(idx);
        visitor->visit(key->expression);
        const auto *key_eval = state->get_expr_result();
        cstring key_name = table_name + "_table_key_" + std::to_string(idx);
        // It is actually possible to use a variety of types as key.
        // So we have to stay generic and produce a corresponding variable.
        auto *key_match =
            visitor->state->gen_instance(key_name, key_eval->get_p4_type());
        hit = hit || (*key_eval == *key_match);
    }
    return hit;
}

void handle_table_action(Z3Visitor *visitor, cstring table_name,
                         const IR::MethodCallExpression *act,
                         cstring action_label) {
    auto *state = visitor->state;
    const IR::Expression *call_name = nullptr;
    IR::Vector<IR::Argument> ctrl_args;
    const IR::ParameterList *method_params = nullptr;

    if (const auto *path = act->method->to<IR::PathExpression>()) {
        call_name = path;
        cstring identifier_path =
            path->path->name + std::to_string(act->arguments->size());
        const auto *action_decl = state->get_static_decl(identifier_path);
        if (const auto *action = action_decl->decl->to<IR::P4Action>()) {
            method_params = action->getParameters();
        } else {
            BUG("Unexpected action call %s of type %s in table.",
                action_decl->decl, action_decl->decl->node_type_name());
        }
    } else {
        P4C_UNIMPLEMENTED("Unsupported action %s of type %s", act,
                          act->method->node_type_name());
    }
    for (const auto &arg : *act->arguments) {
        ctrl_args.push_back(arg);
    }
    // At this stage, we synthesize control plane arguments
    // TODO: Simplify this.
    auto args_len = act->arguments->size();
    auto ctrl_idx = 0;
    for (size_t idx = 0; idx < method_params->size(); ++idx) {
        const auto *param = method_params->getParameter(idx);
        if (args_len <= idx && param->direction == IR::Direction::None) {
            cstring arg_name =
                table_name + action_label + std::to_string(ctrl_idx);
            auto *ctrl_arg = state->gen_instance(arg_name, param->type);
            // TODO: This is a bug waiting to happen. How to handle fresh
            // arguments and their source?
            state->declare_var(arg_name, ctrl_arg, param->type);
            ctrl_args.push_back(
                new IR::Argument(new IR::PathExpression(arg_name)));
            ctrl_idx++;
        }
    }

    const auto *action_with_ctrl_args =
        new IR::MethodCallExpression(call_name, &ctrl_args);
    visitor->visit(action_with_ctrl_args);
}

bool Z3Visitor::preorder(const IR::P4Table *p4t) {
    auto *ctx = state->get_z3_ctx();
    auto table_name = infer_name(p4t->getAnnotations(), p4t->name.name);
    auto table_action_name = table_name + "action_idx";
    auto table_action = ctx->int_const(table_action_name.c_str());
    bool immutable = false;
    // We first collect all the necessary properties
    std::vector<const IR::KeyElement *> keys;
    std::vector<const IR::MethodCallExpression *> actions;
    const IR::MethodCallExpression *default_action = nullptr;
    process_table_properties(p4t, &keys, &actions, &default_action, &immutable);

    z3::expr hit = compute_table_hit(this, table_name, keys);

    std::vector<std::pair<z3::expr, VarMap>> action_vars;
    bool has_exited = true;
    z3::expr matches = ctx->bool_val(false);
    for (size_t idx = 0; idx < actions.size(); ++idx) {
        const auto *action = actions.at(idx);
        auto cond = hit && (table_action == ctx->int_val(idx));
        auto old_vars = state->clone_vars();
        state->push_forward_cond(cond);
        handle_table_action(this, table_name, action, std::to_string(idx));
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        if (!call_has_exited) {
            action_vars.emplace_back(cond, state->get_vars());
        }
        has_exited = has_exited && call_has_exited;
        state->set_exit(false);
        state->restore_vars(old_vars);
        matches = matches || cond;
    }

    if (default_action != nullptr) {
        auto old_vars = state->clone_vars();
        state->push_forward_cond(!matches);
        handle_table_action(this, table_name, default_action, "default");
        state->pop_forward_cond();
        if (state->has_exited()) {
            state->restore_vars(old_vars);
        }
    }
    state->set_exit(has_exited && state->has_exited());

    for (auto it = action_vars.rbegin(); it != action_vars.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    // also check if the table is invisible to the control plane
    // this also implies that it cannot be modified
    auto annos = p4t->getAnnotations()->annotations;
    if (std::any_of(annos.begin(), annos.end(), [](const IR::Annotation *anno) {
            return anno->name.name == "hidden";
        })) {
        immutable = true;
    }
    state->set_expr_result(new P4TableInstance(state, p4t, table_name, hit,
                                               keys, actions, immutable));
    return false;
}

bool Z3Visitor::preorder(const IR::EmptyStatement *) { return false; }

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
    // if we do not even return do not bother with collecting results
    if (r->expression != nullptr) {
        visit(r->expression);
        state->push_return_expr(cond && exit_cond, state->copy_expr_result());
    }
    state->push_return_state(cond && exit_cond, state->clone_vars());
    state->push_return_cond(!cond);
    state->set_returned(true);

    return false;
}

bool Z3Visitor::preorder(const IR::ExitStatement *) {
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
    // Note the lack of leq in the i > 1 comparison.
    // We do not want to pop the last two scopes
    // FIXME: There has to be a cleaner way here...
    // Ideally we should track the input/output variables
    for (int64_t i = scopes.size() - 1; i > 1; --i) {
        const auto *scope = &scopes.at(i);
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

void handle_table_match(P4State *state, Z3Visitor *visitor,
                        const P4TableInstance *table,
                        const IR::Vector<IR::SwitchCase> &cases) {
    auto *ctx = state->get_z3_ctx();
    auto table_action_name = table->table_name + "action_idx";
    auto action_taken = ctx->int_const(table_action_name.c_str());
    std::map<cstring, int> action_mapping;
    size_t idx = 0;
    for (const auto *action : table->actions) {
        const auto *method_expr = action->method;
        const auto *path = method_expr->to<IR::PathExpression>();
        CHECK_NULL(path);
        action_mapping[path->path->name.name] = idx;
        idx++;
    }
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    const IR::SwitchCase *default_case = nullptr;
    // now actually map all the statements together
    bool has_exited = true;
    bool has_returned = true;
    z3::expr fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    for (const auto *switch_case : cases) {
        if (const auto *label = switch_case->label->to<IR::PathExpression>()) {
            auto mapped_idx = action_mapping[label->path->name.name];
            auto cond = action_taken == mapped_idx;
            // There is no block for the switch.
            // This expressions falls through to the next switch case.
            if (switch_case->statement == nullptr) {
                fall_through = fall_through || cond;
                continue;
            }
            // Matches the condition OR all the other fall-through switches
            auto case_match = fall_through || cond;
            fall_through = ctx->bool_val(false);
            auto old_vars = state->clone_vars();
            state->push_forward_cond(case_match);
            visitor->visit(switch_case->statement);
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
            matches = matches || case_match;
        } else if (switch_case->label->is<IR::DefaultExpression>()) {
            default_case = switch_case;
        } else {
            P4C_UNIMPLEMENTED("Case expression %s of type %s not supported.",
                              switch_case->label,
                              switch_case->label->node_type_name());
        }
    }
    if (default_case != nullptr) {
        auto old_vars = state->clone_vars();
        state->push_forward_cond(!matches);
        visitor->visit(default_case->statement);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (call_has_exited || stmt_has_returned) {
            state->restore_vars(old_vars);
        }
        has_exited = has_exited && call_has_exited;
        has_returned = has_returned && stmt_has_returned;
    } else {
        // the empty default switch neither exits nor returns
        has_exited = false;
        has_returned = false;
    }
    state->set_exit(has_exited);
    state->set_returned(has_returned);

    for (auto it = case_states.rbegin(); it != case_states.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
}

bool Z3Visitor::preorder(const IR::SwitchStatement *ss) {
    visit(ss->expression);
    const auto *table = state->copy_expr_result<P4TableInstance>();
    handle_table_match(state, this, table, ss->cases);
    // P4C_UNIMPLEMENTED("Unsupported switch expression %s of type %s.", result,
    //                   result->get_static_type());

    return false;
}

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

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (const auto *c : b->components) {
        visit(c);
        if (state->has_returned() || state->has_exited()) {
            break;
        }
    }
    return false;
}

bool Z3Visitor::preorder(const IR::MethodCallStatement *mcs) {
    visit(mcs->methodCall);
    return false;
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    state->set_var(this, as->left, as->right);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left = nullptr;
    if (dv->initializer != nullptr) {
        visit(dv->initializer);
        left = state->get_expr_result()->cast_allocate(dv->type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, dv->type);
    }
    state->declare_var(dv->name.name, left, dv->type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left = nullptr;
    if (dc->initializer != nullptr) {
        visit(dc->initializer);
        left = state->get_expr_result()->cast_allocate(dc->type);
    } else {
        left = state->gen_instance(UNDEF_LABEL, dc->type);
    }
    state->declare_var(dc->name.name, left, dc->type);
    return false;
}

P4Z3Instance *run_arch_block(Z3Visitor *visitor,
                             const IR::ConstructorCallExpression *cce) {
    auto *state = visitor->state;
    state->push_scope();

    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    const IR::ParameterList *params = nullptr;
    if (const auto *c = resolved_type->to<IR::P4Control>()) {
        params = c->getApplyParameters();
    } else if (const auto *p = resolved_type->to<IR::P4Parser>()) {
        params = p->getApplyParameters();
    } else if (const auto *ext = resolved_type->to<IR::Type_Extern>()) {
        // TODO: What are params here?
        params = new IR::ParameterList();
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                          resolved_type, resolved_type->node_type_name());
    }
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    std::vector<cstring> state_names;

    // INITIALIZE
    // TODO: Simplify this
    IR::Vector<IR::Argument> synthesized_args;
    for (const auto *param : *params) {
        const auto *par_type = state->resolve_type(param->type);
        if (!par_type->is<IR::Type_Package>()) {
            auto *var = state->gen_instance(param->name.name, par_type);
            if (param->direction != IR::Direction::Out) {
                if (auto *z3_var = var->to_mut<StructBase>()) {
                    z3_var->bind(0, param->name.name);
                    z3_var->propagate_validity();
                }
            }
            state->declare_var(param->name.name, var, par_type);
        }
        state_names.push_back(param->name.name);
        const auto *arg =
            new IR::Argument(new IR::PathExpression(param->name.name));
        synthesized_args.push_back(arg);
    }
    const auto new_cce =
        IR::ConstructorCallExpression(cce->constructedType, &synthesized_args);
    const Visitor_Context *visitor_ctx = nullptr;
    if (visitor->getChildContext() != nullptr) {
        visitor_ctx = visitor->getContext();
    }
    new_cce.apply(*visitor, visitor_ctx);

    // Merge the exit states
    state->merge_exit_states();

    // COLLECT
    for (auto state_name : state_names) {
        const auto *var = state->get_var(state_name);
        if (const auto *z3_var = var->to<NumericVal>()) {
            state_vars.emplace_back(state_name, *z3_var->get_val());
        } else if (const auto *z3_var = var->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(state_name);
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                              z3_sub_vars.end());
        } else if (var->is<ExternInstance>()) {
            warning("Skipping extern because we do not know how to represent "
                    "it.");
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }

    state->pop_scope();

    return new ControlState(state_vars);
}

VarMap create_state(Z3Visitor *visitor, const IR::Vector<IR::Argument> *args,
                    const IR::ParameterList *params) {
    VarMap merged_vec;
    size_t idx = 0;

    ordered_map<const IR::Parameter *, const IR::Expression *> param_mapping;
    for (const auto &param : *params) {
        // This may have a nullptr, but we need to maintain order
        param_mapping.emplace(param, param->defaultValue);
    }
    for (const auto &arg : *args) {
        // We override the mapping here.
        if (arg->name) {
            param_mapping[params->getParameter(arg->name.name)] =
                arg->expression;
        } else {
            param_mapping[params->getParameter(idx)] = arg->expression;
        }
        idx++;
    }

    for (const auto &mapping : param_mapping) {
        const auto *param = mapping.first;
        const auto *arg_expr = mapping.second;
        // Ignore empty optional parameters, they can not be used properly
        if (param->isOptional() && arg_expr == nullptr) {
            continue;
        }
        CHECK_NULL(arg_expr);
        if (const auto *cce = arg_expr->to<IR::ConstructorCallExpression>()) {
            auto *state_result = run_arch_block(visitor, cce);
            merged_vec.insert({param->name.name, {state_result, param->type}});
        } else if (const auto *path = arg_expr->to<IR::PathExpression>()) {
            const auto *decl =
                visitor->state->get_static_decl(path->path->name.name);
            const auto *di = decl->decl->to<IR::Declaration_Instance>();
            CHECK_NULL(di);
            auto sub_results = visitor->gen_state_from_instance(di);
            for (const auto &sub_result : sub_results) {
                auto merged_name = param->name.name + sub_result.first;
                auto variables = sub_result.second;
                merged_vec.insert({merged_name, variables});
            }
        } else if (const auto *cst = arg_expr->to<IR::Literal>()) {
            cst->apply(*visitor);
            merged_vec.insert(
                {param->name.name,
                 {visitor->state->copy_expr_result(), param->type}});
        } else {
            P4C_UNIMPLEMENTED("Unsupported main argument %s of type %s",
                              arg_expr, arg_expr->node_type_name());
        }
    }
    return merged_vec;
}

const IR::ParameterList *get_params(const IR::Type *callable_type) {
    if (const auto *control = callable_type->to<IR::P4Control>()) {
        return control->getConstructorParameters();
    }
    if (const auto *parser = callable_type->to<IR::P4Parser>()) {
        return parser->getConstructorParameters();
    }
    if (const auto *package = callable_type->to<IR::Type_Package>()) {
        return package->getParameters();
    }
    P4C_UNIMPLEMENTED("Callable declaration type %s of type %s not supported.",
                      callable_type, callable_type->node_type_name());
}

VarMap Z3Visitor::gen_state_from_instance(const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (const auto *spec_type = resolved_type->to<IR::Type_Specialized>()) {
        // FIXME: Figure out what do here
        // for (auto arg : *spec_type->arguments) {
        //     const IR::Type *resolved_arg = state->resolve_type(arg);
        // }
        resolved_type = state->resolve_type(spec_type->baseType);
    }
    const auto *params = get_params(resolved_type);
    return create_state(this, di->arguments, params);
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (const auto *spec_type = resolved_type->to<IR::Type_Specialized>()) {
        // FIXME: Figure out what do here
        // for (auto arg : *spec_type->arguments) {
        //     const IR::Type *resolved_arg = state->resolve_type(arg);
        // }
        resolved_type = state->resolve_type(spec_type->baseType);
    }

    if (const auto *instance_decl = resolved_type->to<IR::Type_Declaration>()) {
        const IR::ParameterList *params = nullptr;
        if (const auto *c = instance_decl->to<IR::P4Control>()) {
            params = c->getConstructorParameters();
        } else if (const auto *p = instance_decl->to<IR::P4Parser>()) {
            params = p->getConstructorParameters();
        } else {
            P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                              resolved_type, resolved_type->node_type_name());
        }
        auto var_map =
            state->merge_args_with_const_params(this, *di->arguments, *params);
        state->declare_var(di->name.name,
                           new ControlInstance(state, instance_decl, var_map),
                           resolved_type);
    } else {
        P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ",
                          resolved_type, resolved_type->node_type_name());
    }
    return false;
}

}  // namespace TOZ3
