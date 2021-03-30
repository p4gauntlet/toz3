#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

bool Z3Visitor::preorder(const IR::P4Control *c) {
    TypeVisitor map_builder = TypeVisitor(state);

    for (const IR::Declaration *local_decl : c->controlLocals) {
        local_decl->apply(map_builder);
    }

    // DO SOMETHING
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
        auto return_type = f->type->returnType;
        auto merged_return = begin->second->cast_allocate(return_type);
        for (auto it = std::next(begin); it != end; ++it) {
            z3::expr cond = it->first;
            auto then_var = it->second->cast_allocate(return_type);
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
    auto method_name = m->getName().name;
    auto method_type = m->type->returnType;
    // TODO: Different types of arguments and multiple calls
    for (auto param : *m->getParameters()) {
        cstring param_name = param->getName().name;
        cstring merged_param_name = method_name + "_" + param_name;
        if (param->direction == IR::Direction::Out ||
            param->direction == IR::Direction::InOut) {
            auto instance = state->gen_instance(merged_param_name, param->type);
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

    for (auto p : p4t->properties->properties) {
        auto val = p->value;
        if (auto key = val->to<IR::Key>()) {
            for (auto ke : key->keyElements) {
                keys->push_back(ke);
            }
        } else if (auto action_list = val->to<IR::ActionList>()) {
            for (auto act : action_list->actionList) {
                bool ignore_default = false;
                for (const auto *anno : act->getAnnotations()->annotations) {
                    if (anno->name.name == "defaultonly") {
                        ignore_default = true;
                    }
                }
                if (ignore_default)
                    continue;
                if (auto method_call =
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
        } else if (auto expr_val = val->to<IR::ExpressionValue>()) {
            // resolve a default action
            if (p->name.name == "default_action") {
                if (auto method_call =
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
        if (p->name.name == "entries" and p->isConstant) {
            *immutable = true;
        }
    }
}

z3::expr compute_table_hit(Z3Visitor *visitor, cstring table_name,
                           const std::vector<const IR::KeyElement *> &keys) {
    auto state = visitor->state;
    auto ctx = state->get_z3_ctx();
    z3::expr hit = ctx->bool_val(false);
    for (std::size_t idx = 0; idx < keys.size(); ++idx) {
        auto key = keys.at(idx);
        visitor->visit(key->expression);
        auto key_eval = state->get_expr_result<Z3Bitvector>();
        cstring key_name = table_name + "_table_key_" + std::to_string(idx);
        auto key_match =
            ctx->bv_const(key_name.c_str(), key_eval->val.get_sort().bv_size());
        hit = hit || (key_eval->val == key_match);
    }
    return hit;
}

void handle_table_action(Z3Visitor *visitor, cstring table_name,
                         const IR::MethodCallExpression *act,
                         cstring action_label) {
    auto state = visitor->state;
    const IR::Expression *call_name;
    IR::Vector<IR::Argument> ctrl_args;
    const IR::ParameterList *method_params;

    if (auto path = act->method->to<IR::PathExpression>()) {
        call_name = path;
        cstring identifier_path =
            path->path->name + std::to_string(act->arguments->size());
        auto action_decl = state->get_static_decl(identifier_path);
        if (auto action = action_decl->decl->to<IR::P4Action>()) {
            method_params = action->getParameters();
        } else {
            BUG("Unexpected action call %s of type %s in table.",
                action_decl->decl, action_decl->decl->node_type_name());
        }
    } else {
        P4C_UNIMPLEMENTED("Unsupported action %s of type %s", act,
                          act->method->node_type_name());
    }
    for (auto &arg : *act->arguments) {
        ctrl_args.push_back(arg);
    }
    // At this stage, we synthesize control plane arguments
    auto args_len = act->arguments->size();
    for (size_t idx = 0; idx < method_params->size(); ++idx) {
        auto param = method_params->getParameter(idx);
        if (args_len <= idx) {
            cstring arg_name = table_name + action_label + std::to_string(idx);
            auto ctrl_arg = state->gen_instance(arg_name, param->type);
            // TODO: This is a bug waiting to happen. How to handle fresh
            // arguments and their source?
            state->declare_var(arg_name, ctrl_arg, param->type);
            ctrl_args.push_back(
                new IR::Argument(new IR::PathExpression(arg_name)));
        }
    }
    const auto action_with_ctrl_args =
        new IR::MethodCallExpression(call_name, &ctrl_args);
    visitor->visit(action_with_ctrl_args);
}

bool Z3Visitor::preorder(const IR::P4Table *p4t) {
    auto ctx = state->get_z3_ctx();
    auto table_name = infer_name(p4t->getAnnotations(), p4t->getName().name);
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
        auto action = actions.at(idx);
        auto cond = hit && (table_action == ctx->int_val(idx));
        auto old_vars = state->clone_vars();
        state->push_forward_cond(cond);
        handle_table_action(this, table_name, action, std::to_string(idx));
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        if (!call_has_exited) {
            action_vars.push_back({cond, state->get_vars()});
        }
        has_exited = has_exited && call_has_exited;
        state->set_exit(false);
        state->restore_vars(old_vars);
        matches = matches || cond;
    }

    if (default_action) {
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
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (z3::expr sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();
    // if we do not even return do not bother with collecting results
    if (r->expression) {
        visit(r->expression);
        state->push_return_expr(cond && exit_cond, state->copy_expr_result());
    }
    state->push_return_state(cond && exit_cond, state->clone_vars());
    state->set_exit_cond(exit_cond && !cond);
    state->set_returned(true);

    return false;
}

bool Z3Visitor::preorder(const IR::ExitStatement *) {
    auto forward_conds = state->get_forward_conds();
    auto cond = state->get_z3_ctx()->bool_val(true);
    for (z3::expr sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    auto exit_cond = state->get_exit_cond();

    auto scopes = state->get_state();
    auto old_state = state->clone_state();
    // Note the lack of leq in the i > 1 comparison.
    // We do not want to pop the last two scopes
    // FIXME: There has to be a cleaner way here...
    // Ideally we should track the input/output variables
    for (int i = scopes.size() - 1; i > 1; --i) {
        auto scope = &scopes.at(i);
        auto copy_out_args = scope->get_copy_out_args();
        std::vector<P4Z3Instance *> copy_out_vals;
        for (auto arg_tuple : copy_out_args) {
            auto source = arg_tuple.second;
            auto val = state->get_var(source);
            copy_out_vals.push_back(val);
        }

        state->pop_scope();
        size_t idx = 0;
        for (auto arg_tuple : copy_out_args) {
            auto target = &arg_tuple.first;
            state->set_var(target, copy_out_vals[idx]);
            idx++;
        }
    }
    auto exit_vars = state->clone_vars();
    state->restore_state(old_state);
    state->exit_states.push_back({exit_cond && cond, exit_vars});
    state->set_exit_cond(exit_cond && !cond);
    state->set_exit(true);

    return false;
}

void handle_table_match(P4State *state, Z3Visitor *visitor,
                        const P4TableInstance *table,
                        const IR::Vector<IR::SwitchCase> &cases) {
    auto ctx = state->get_z3_ctx();
    auto table_action_name = table->table_name + "action_idx";
    auto action_taken = ctx->int_const(table_action_name.c_str());
    std::map<cstring, int> action_mapping;
    size_t idx = 0;
    for (auto action : table->actions) {
        auto method_expr = action->method;
        auto path = method_expr->to<IR::PathExpression>();
        CHECK_NULL(path);
        action_mapping[path->path->name.name] = idx;
        idx++;
    }
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    const IR::SwitchCase *default_case = nullptr;
    // now actually map all the statements together
    bool has_exited = true;
    bool has_returned = true;
    bool fall_through = ctx->bool_val(false);
    z3::expr matches = ctx->bool_val(false);
    for (auto switch_case : cases) {
        if (auto label = switch_case->label->to<IR::PathExpression>()) {
            auto mapped_idx = action_mapping[label->path->name.name];
            auto cond = action_taken == mapped_idx;
            if (!switch_case->statement) {
                fall_through = fall_through || cond;
                continue;
            }
            fall_through = ctx->bool_val(false);
            auto old_vars = state->clone_vars();
            state->push_forward_cond(cond);
            visitor->visit(switch_case->statement);
            state->pop_forward_cond();
            auto call_has_exited = state->has_exited();
            auto stmt_has_returned = state->has_returned();
            if (!(call_has_exited or stmt_has_returned)) {
                case_states.push_back({cond, state->get_vars()});
            }
            has_exited = has_exited && call_has_exited;
            has_returned = has_returned && stmt_has_returned;
            state->set_exit(false);
            state->set_returned(false);
            state->restore_vars(old_vars);
            matches = matches || cond;
        } else if (switch_case->label->is<IR::DefaultExpression>()) {
            default_case = switch_case;
        } else {
            P4C_UNIMPLEMENTED("Case expression %s of type %s not supported.",
                              switch_case->label,
                              switch_case->label->node_type_name());
        }
    }
    if (default_case) {
        auto old_vars = state->clone_vars();
        state->push_forward_cond(!matches);
        visitor->visit(default_case->statement);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (call_has_exited or stmt_has_returned) {
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
    auto table = state->copy_expr_result<P4TableInstance>();
    handle_table_match(state, this, table, ss->cases);
    // P4C_UNIMPLEMENTED("Unsupported switch expression %s of type %s.", result,
    //                   result->get_static_type());

    return false;
}

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto z3_cond = state->get_expr_result<Z3Bitvector>()->val.simplify();
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
    if (then_has_exited or then_has_returned) {
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
    if (else_has_exited or else_has_returned) {
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
    for (auto c : b->components) {
        visit(c);
        if (state->has_returned() or state->has_exited()) {
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
    P4Z3Instance *left;
    if (dv->initializer) {
        visit(dv->initializer);
        left = state->get_expr_result()->cast_allocate(dv->type);
    } else {
        left = state->gen_instance("undefined", dv->type);
    }
    state->declare_var(dv->name.name, left, dv->type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Constant *dc) {
    P4Z3Instance *left;
    if (dc->initializer) {
        visit(dc->initializer);
        left = state->get_expr_result()->cast_allocate(dc->type);
    } else {
        left = state->gen_instance("undefined", dc->type);
    }
    state->declare_var(dc->name.name, left, dc->type);
    return false;
}

const IR::ParameterList *get_params(const IR::Type *callable_type) {
    if (auto control = callable_type->to<IR::P4Control>()) {
        return control->getApplyParameters();
    } else if (auto parser = callable_type->to<IR::P4Parser>()) {
        return parser->getApplyParameters();
    } else if (auto package = callable_type->to<IR::Type_Package>()) {
        return package->getParameters();
    } else {
        P4C_UNIMPLEMENTED(
            "Callable declaration type %s of type %s not supported.",
            callable_type, callable_type->node_type_name());
    }
}

P4Z3Instance *run_arch_block(Z3Visitor *visitor,
                             const IR::ConstructorCallExpression *cce) {
    auto state = visitor->state;
    state->push_scope();

    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    const IR::ParameterList *params;
    if (auto c = resolved_type->to<IR::P4Control>()) {
        params = c->getApplyParameters();
    } else if (auto p = resolved_type->to<IR::P4Parser>()) {
        params = p->getApplyParameters();
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.",
                          resolved_type, resolved_type->node_type_name());
    }
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    std::vector<cstring> state_names;

    // INITIALIZE
    IR::Vector<IR::Argument> synthesized_args;
    for (auto param : *params) {
        auto par_type = state->resolve_type(param->type);
        auto var = state->gen_instance(param->name.name, par_type);
        if (auto z3_var = var->to_mut<StructInstance>()) {
            z3_var->propagate_validity();
        }
        state->declare_var(param->name.name, var, par_type);
        state_names.push_back(param->name.name);
        auto arg = new IR::Argument(new IR::PathExpression(param->name.name));
        synthesized_args.push_back(arg);
    }
    const auto new_cce = new IR::ConstructorCallExpression(cce->constructedType,
                                                           &synthesized_args);
    visitor->visit(new_cce);

    // Merge the exit states
    for (auto exit_tuple : state->exit_states) {
        state->merge_vars(exit_tuple.first, exit_tuple.second);
    }
    // Clear the exit states
    state->exit_states.clear();

    // COLLECT
    for (auto state_name : state_names) {
        auto var = state->get_var(state_name);
        if (auto z3_var = var->to<Z3Bitvector>()) {
            state_vars.push_back({state_name, z3_var->val});
        } else if (auto z3_var = var->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(state_name);
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                              z3_sub_vars.end());
        } else if (var->to<ExternInstance>()) {
            printf("Skipping extern...\n");
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
    size_t arg_len = args->size();
    size_t idx = 0;
    for (auto param : params->parameters) {
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            if (auto cce =
                    arg->expression->to<IR::ConstructorCallExpression>()) {
                auto state_result = run_arch_block(visitor, cce);
                merged_vec.insert(
                    {param->name.name, {state_result, param->type}});
            } else {
                P4C_UNIMPLEMENTED("Unsupported main argument %s",
                                  arg->expression);
            }
        } else {
            BUG("Mismatch between arguments %s and parameters %s", *args,
                *params);
        }
        idx++;
    }

    return merged_vec;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    auto instance_name = di->getName().name;
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (auto spec_type = resolved_type->to<IR::Type_Specialized>()) {
        // FIXME: Figure out what do here
        // for (auto arg : *spec_type->arguments) {
        //     const IR::Type *resolved_arg = state->resolve_type(arg);
        // }
        resolved_type = state->resolve_type(spec_type->baseType);
    }
    const IR::ParameterList *params = get_params(resolved_type);

    if (instance_name == "main") {
        main_result = create_state(this, di->arguments, params);
    } else {
        state->merge_args_with_params(this, di->arguments, params);
        if (auto instance_decl = resolved_type->to<IR::Type_Declaration>()) {
            state->declare_var(instance_name,
                               new DeclarationInstance(state, instance_decl),
                               resolved_type);
        } else {
            P4C_UNIMPLEMENTED("Resolved type %s of type %s not supported, ",
                              resolved_type, resolved_type->node_type_name());
        }
    }

    return false;
}

} // namespace TOZ3_V2
