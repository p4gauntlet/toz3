#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

cstring infer_name(const IR::Annotations *annots, cstring default_name) {
    // This function is a bit of a hacky way to infer the true name of a
    // declaration. Since there are a couple of passes that rename but add
    // annotations we can infer the original name from the annotation.
    // not sure if this generalizes but this is as close we can get for now
    for (auto anno : annots->annotations) {
        // there is an original name in the form of an annotation
        if (anno->name.name == "name") {
            for (auto token : anno->body) {
                // the full name can be a bit more convoluted
                // we only need the last bit after the dot
                // so hack it out
                cstring full_name = token->text;

                // find the last dot
                const char *last_dot = full_name.findlast((int)'.');
                // there is no dot in this string, just return the full name
                if (not last_dot) {
                    return full_name;
                }
                // otherwise get the index, remove the dot
                size_t idx = (size_t)(last_dot - full_name + 1);
                return token->text.substr(idx);
            }
            // if the annotation is a member just get the root name
            if (auto member = anno->expr.to<IR::Member>()) {
                return member->member.name;
            }
        }
    }

    return default_name;
}

Visitor::profile_t Z3Visitor::init_apply(const IR::Node *node) {
    return Inspector::init_apply(nullptr);
}

void Z3Visitor::end_apply(const IR::Node *) {}

bool Z3Visitor::preorder(const IR::P4Control *c) {
    TypeVisitor map_builder = TypeVisitor(state);

    for (const IR::Declaration *local_decl : c->controlLocals) {
        local_decl->apply_visitor_preorder(map_builder);
    }

    // DO SOMETHING
    visit(c->body);
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
    for (auto p : p4t->properties->properties) {
        auto val = p->value;
        if (auto key = val->to<IR::Key>()) {
            for (auto ke : key->keyElements) {
                keys.push_back(ke);
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
                    actions.push_back(method_call);
                } else if (auto path =
                               act->expression->to<IR::PathExpression>()) {
                    auto method_call = new IR::MethodCallExpression(path);
                    actions.push_back(method_call);
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
                    default_action = method_call;
                } else if (auto path =
                               expr_val->expression->to<IR::PathExpression>()) {
                    default_action = new IR::MethodCallExpression(path);
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
            immutable = true;
        }
    }
    z3::expr hit = ctx->bool_val(false);
    for (std::size_t idx = 0; idx < keys.size(); ++idx) {
        auto key = keys.at(idx);
        (key->expression)->apply_visitor_preorder(expr_resolver);
        auto key_eval = state->get_expr_result<Z3Bitvector>();
        cstring key_name = table_name + "_table_key_" + std::to_string(idx);
        auto key_match =
            ctx->bv_const(key_name.c_str(), key_eval->val.get_sort().bv_size());
        hit = hit || (key_eval->val == key_match);
    }

    std::vector<std::pair<z3::expr, VarMap>> action_vars;

    bool has_exited = true;
    z3::expr matches = ctx->bool_val(false);
    for (std::size_t idx = 0; idx < actions.size(); ++idx) {
        auto action = actions.at(idx);
        auto cond = hit && (table_action == ctx->int_val(idx));
        auto old_vars = state->clone_vars();
        state->push_forward_cond(cond);
        visit(action);
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
    auto old_vars = state->clone_vars();
    state->push_forward_cond(!matches);
    visit(default_action);
    state->pop_forward_cond();
    if (state->has_exited()) {
        state->restore_vars(old_vars);
    }
    state->set_exit(state->has_exited() && has_exited);
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
        (r->expression)->apply_visitor_preorder(expr_resolver);
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
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        auto copy_out_args = scope->get_copy_out_args();
        for (auto arg_tuple : copy_out_args) {
            auto target = arg_tuple.first;
            auto out_val = state->get_var(arg_tuple.second);
            state->pop_scope();
            set_var(state, target, out_val);
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
    (ss->expression)->apply_visitor_preorder(expr_resolver);
    auto table = state->copy_expr_result<P4TableInstance>();
    handle_table_match(state, this, table, ss->cases);
    // P4C_UNIMPLEMENTED("Unsupported switch expression %s of type %s.", result,
    //                   result->get_static_type());

    return false;
}

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    (ifs->condition)->apply_visitor_preorder(expr_resolver);
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
    as->right->apply_visitor_preorder(expr_resolver);
    set_var(state, as->left, state->get_expr_result());
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    P4Z3Instance *left;
    if (dv->initializer) {
        dv->initializer->apply_visitor_preorder(expr_resolver);
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
        dc->initializer->apply_visitor_preorder(expr_resolver);
        left = state->get_expr_result()->cast_allocate(dc->type);
    } else {
        left = state->gen_instance("undefined", dc->type);
    }
    state->declare_var(dc->name.name, left, dc->type);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = state->resolve_type(di->type);
    if (auto pkt_type = resolved_type->to<IR::Type_Package>()) {
        decl_result = merge_args_with_params(&expr_resolver, di->arguments,
                                             pkt_type->getParameters());
    } else if (auto spec_type = resolved_type->to<IR::Type_Specialized>()) {
        const IR::Type *resolved_base_type =
            state->resolve_type(spec_type->baseType);
        if (auto pkt_type = resolved_base_type->to<IR::Type_Package>()) {
            decl_result = merge_args_with_params(&expr_resolver, di->arguments,
                                                 pkt_type->getParameters());
            // FIXME: Figure out what do here
            // for (auto arg : *spec_type->arguments) {
            //     const IR::Type *resolved_arg = state->resolve_type(arg);
            // }
        } else {
            P4C_UNIMPLEMENTED("Specialized type %s not supported.",
                              resolved_base_type->node_type_name());
        }
    } else {
        P4C_UNIMPLEMENTED("Declaration Instance Type %s not supported.",
                          resolved_type->node_type_name());
    }
    return false;
}

} // namespace TOZ3_V2
