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
    return Inspector::init_apply(node);
}

void Z3Visitor::end_apply(const IR::Node *) {}

void Z3Visitor::fill_with_z3_sorts(std::vector<const IR::Node *> *sorts,
                                   const IR::Type *t) {
    t = state->resolve_type(t);
    sorts->push_back(t);
}

P4Z3Result
Z3Visitor::merge_args_with_params(const IR::Vector<IR::Argument> *args,
                                  const IR::ParameterList *params) {
    P4Z3Result merged_vec;
    size_t arg_len = args->size();
    size_t idx = 0;
    for (auto param : params->parameters) {
        if (param->direction == IR::Direction::Out) {
            auto instance = state->gen_instance("undefined", param->type);
            merged_vec.insert({param->name.name, instance});
            continue;
        }
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            visit(arg->expression);
            merged_vec.insert({param->name.name, state->copy_expr_result()});
        } else {
            auto arg_expr = state->gen_instance(param->name.name, param->type);
            merged_vec.insert({param->name.name, arg_expr});
        }
        idx++;
    }

    return merged_vec;
}

bool Z3Visitor::preorder(const IR::P4Control *c) {

    TypeVisitor map_builder = TypeVisitor(state);

    for (const IR::Declaration *local_decl : c->controlLocals) {
        local_decl->apply(map_builder);
    }

    // DO SOMETHING
    visit(c->body);
    return false;
}

bool Z3Visitor::preorder(const IR::P4Action *a) {
    visit(a->body);
    return false;
}

bool Z3Visitor::preorder(const IR::Function *f) {
    state->return_exprs.clear();
    visit(f->body);

    auto begin = state->return_exprs.rbegin();
    auto end = state->return_exprs.rend();
    if (begin == end) {
        return false;
    }
    auto return_type = f->type->returnType;
    P4Z3Instance *merged_return = begin->second.cast_allocate(return_type);
    for (auto it = std::next(begin); it != end; ++it) {
        z3::expr cond = it->first;
        auto then_var = it->second.cast_allocate(return_type);
        merged_return->merge(cond, *then_var);
    }
    state->set_expr_result(merged_return);
    state->return_exprs.clear();
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
    // Only set a return value if the method has something to return
    if (!method_type->is<IR::Type_Void>()) {
        state->set_expr_result(state->gen_instance(method_name, method_type));
    }
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
                } else {
                    P4C_UNIMPLEMENTED(
                        "Unsupported expression value %s of type %s", expr_val,
                        expr_val->expression->node_type_name());
                }
            }
            printf("ExpressionValue property %s of type %s\n",
                   expr_val->expression->toString().c_str(),
                   expr_val->expression->node_type_name().c_str());
        } else {
            printf("Unknown property %s of type %s\n", p->toString().c_str(),
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
        visit(key->expression);
        auto key_eval = state->get_expr_result<Z3Bitvector>();
        cstring key_name = table_name + "_table_key_" + std::to_string(idx);
        auto key_match =
            ctx->bv_const(key_name.c_str(), key_eval->val.get_sort().bv_size());
        hit = hit || (key_eval->val == key_match);
    }
    std::vector<std::pair<z3::expr, ProgState>> action_states;

    for (std::size_t idx = 0; idx < actions.size(); ++idx) {
        auto action = actions.at(idx);
        auto cond = table_action == ctx->int_val(idx);
        ProgState old_state = state->fork_state();
        state->get_current_scope()->push_forward_cond(&cond);
        visit(action);
        action_states.push_back({cond, state->copy_state()});
        state->restore_state(&old_state);
    }
    visit(default_action);

    for (auto it = action_states.rbegin(); it != action_states.rend(); ++it) {
        state->merge_state(it->first, it->second);
    }
    // also check if the table is invisible to the control plane
    // this also implies that it cannot be modified
    auto annos = p4t->getAnnotations()->annotations;
    if (std::any_of(annos.begin(), annos.end(), [](const IR::Annotation *anno) {
            return anno->name.name == "hidden";
        })) {
        immutable = true;
    }

    return false;
}

bool Z3Visitor::preorder(const IR::EmptyStatement *) { return false; }

bool Z3Visitor::preorder(const IR::ReturnStatement *r) {
    auto forward_conds = state->get_current_scope()->get_forward_conds();

    z3::expr cond = state->get_z3_ctx()->bool_val(true);
    for (z3::expr sub_cond : forward_conds) {
        cond = cond && sub_cond;
    }
    visit(r->expression);
    state->return_exprs.push_back({cond, *state->copy_expr_result()});
    state->get_current_scope()->set_returned(true);

    return false;
}

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto cond = state->copy_expr_result();
    auto z3_cond = cond->to<Z3Bitvector>();
    if (!z3_cond) {
        BUG("Unsupported condition type.");
    }

    ProgState old_state = state->fork_state();
    state->get_current_scope()->push_forward_cond(&z3_cond->val);
    visit(ifs->ifTrue);
    ProgState then_state = state->copy_state();
    state->restore_state(&old_state);
    state->get_current_scope()->push_forward_cond(&z3_cond->val);
    visit(ifs->ifFalse);
    state->get_current_scope()->pop_forward_cond();

    // If both branches have returned we set the if statement to returned
    state->get_current_scope()->set_returned(old_state.back().has_returned() &&
                                             then_state.back().has_returned());

    state->merge_state(z3_cond->val, then_state);
    return false;
}

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (auto c : b->components) {
        visit(c);
        if (state->get_current_scope()->has_returned()) {
            break;
        }
    }
    return false;
}

bool Z3Visitor::preorder(const IR::MethodCallStatement *mcs) {
    visit(mcs->methodCall);
    return false;
}

void Z3Visitor::set_var(const IR::Expression *target, P4Z3Instance &val) {
    if (auto name = target->to<IR::PathExpression>()) {
        if (auto mut_int = val.to_mut<Z3Int>()) {
            // FIXME: This is a mess that should not exist
            auto source_var = state->get_var(name->path->name);
            if (auto dst_var = source_var->to<Z3Bitvector>()) {
                auto sort = dst_var->val.get_sort();
                auto cast_val = mut_int->cast_allocate(sort);
                state->update_var(name->path->name, cast_val);
            } else {
                BUG("CAST NOT SUPPORTED>>>>>>");
            }
        } else {
            state->update_var(name->path->name, &val);
        }

    } else if (auto member = target->to<IR::Member>()) {
        visit(member->expr);
        auto complex_class = state->get_expr_result();
        auto si = complex_class->to_mut<StructBase>();
        if (!si) {
            BUG("Can not cast to StructBase.");
        }
        if (val.is<Z3Int>()) {
            // We must cast Ints to avoid Z3 being slow
            // As they are compile constants we are free to do that
            auto dst_type = si->get_member_type(member->member.name);
            auto cast_val = val.cast_allocate(dst_type);
            si->update_member(member->member.name, cast_val);
        } else {
            si->update_member(member->member.name, &val);
        }
    } else {
        BUG("Unknown target %s!", target->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    visit(as->right);
    set_var(as->left, *state->copy_expr_result());
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
    state->declare_local_var(dv->name.name, left);
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
    state->declare_local_var(dc->name.name, left);
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {

    state->push_scope();
    const IR::Type *resolved_type = state->resolve_type(di->type);

    if (auto pkt_type = resolved_type->to<IR::Type_Package>()) {
        decl_result =
            merge_args_with_params(di->arguments, pkt_type->getParameters());
    } else if (auto spec_type = resolved_type->to<IR::Type_Specialized>()) {
        const IR::Type *resolved_base_type =
            state->resolve_type(spec_type->baseType);
        if (auto pkt_type = resolved_base_type->to<IR::Type_Package>()) {
            decl_result = merge_args_with_params(di->arguments,
                                                 pkt_type->getParameters());
            // FIXME: Figure out what do here
            // for (auto arg : *spec_type->arguments) {
            //     const IR::Type *resolved_arg = state->resolve_type(arg);
            // }
        } else {
            BUG("Specialized type %s not supported.",
                resolved_base_type->node_type_name());
        }
    } else {
        BUG("Declaration Instance Type %s not supported.",
            resolved_type->node_type_name());
    }
    state->pop_scope();
    return false;
}

} // namespace TOZ3_V2
