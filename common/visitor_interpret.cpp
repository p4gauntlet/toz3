#include <cstdio>
#include <utility>

#include "lib/exceptions.h"
#include "visitor_fill_type.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

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
    P4Z3Instance *merged_return = cast(state, begin->second, return_type);
    for (auto it = std::next(begin); it != end; ++it) {
        z3::expr cond = it->first;
        P4Z3Instance *then_var = cast(state, it->second, return_type);
        merged_return->merge(&cond, then_var);
    }
    state->set_expr_result(merged_return);
    state->return_exprs.clear();
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
    state->return_exprs.push_back({cond, state->get_expr_result()});
    state->get_current_scope()->set_returned(true);

    return false;
}

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto cond = state->get_expr_result();
    auto z3_cond = cond->to_mut<Z3Wrapper>();
    if (!z3_cond) {
        BUG("Unsupported condition type.");
    }

    ProgState old_state = state->fork_state();
    state->get_current_scope()->push_forward_cond(&z3_cond->val);
    visit(ifs->ifTrue);
    ProgState then_state = state->get_state();
    state->restore_state(&old_state);
    state->get_current_scope()->push_forward_cond(&z3_cond->val);
    visit(ifs->ifFalse);
    state->get_current_scope()->pop_forward_cond();

    // If both branches have returned we set the if statement to returned
    state->get_current_scope()->set_returned(old_state.back().has_returned() &&
                                             then_state.back().has_returned());

    state->merge_state(z3_cond->val, &then_state);
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

void Z3Visitor::set_var(const IR::Expression *target, P4Z3Instance *val) {
    if (auto name = target->to<IR::PathExpression>()) {
        if (auto mut_int = val->to_mut<Z3Int>()) {
            //FIXME: This is a mess that should not exist
            auto source_var = state->get_var(name->path->name);
            if (auto dst_var = source_var->to<Z3Wrapper>()) {
                auto z3_sort = dst_var->val.get_sort();
                mut_int->val = cast(state, val, &z3_sort);
            } else {
                BUG("CAST NOT SUPPORTED>>>>>>");
            }
        }

        state->update_var(name->path->name, val);
    } else if (auto member = target->to<IR::Member>()) {
        visit(member->expr);
        P4Z3Instance *complex_class = state->get_expr_result();
        auto si = complex_class->to_mut<StructBase>();
        if (!si) {
            BUG("Can not cast to StructBase.");
        }
        if (val->is<Z3Int>()) {
            auto dst_type = si->get_member_type(member->member.name);
            val = cast(state, val, dst_type);
        }
        // We must cast Ints to avoid Z3 being slow
        // As they are compile constants we are free to do that
        si->update_member(member->member.name, val);
    } else {
        BUG("Unknown target %s!", target->node_type_name());
    }
}

std::function<void(void)>
Z3Visitor::get_method_member(const IR::Member *member) {
    visit(member->expr);
    P4Z3Instance *complex_class = state->get_expr_result();
    if (auto si = complex_class->to_mut<StructBase>()) {
        return si->get_function(member->member.name);
    } else {
        BUG("Method member not supported.");
    }
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    visit(as->right);
    set_var(as->left, state->copy_expr_result());
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    // TODO: Casting
    P4Z3Instance *left;
    if (dv->initializer) {
        visit(dv->initializer);
        left = state->get_expr_result();
    } else {
        left = state->gen_instance("undefined", dv->type);
    }
    state->declare_local_var(dv->name.name, left);
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
