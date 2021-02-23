#include <cstdio>
#include <utility>

#include "complex_type.h"
#include "lib/exceptions.h"
#include "type_map.h"
#include "z3_int.h"
#include "z3_interpreter.h"

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
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            visit(arg->expression);
            merged_vec.insert({param->name.name, state->return_expr});
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

bool Z3Visitor::preorder(const IR::Declaration_Variable *dv) {
    // TODO: Casting
    visit(dv->initializer);
    state->declare_local_var(dv->name.name, state->return_expr);
    return false;
}

bool Z3Visitor::preorder(const IR::EmptyStatement *) { return false; }

bool Z3Visitor::preorder(const IR::IfStatement *ifs) {
    visit(ifs->condition);
    auto cond = state->return_expr;
    std::vector<P4Scope *> saved_state = state->checkpoint();
    visit(ifs->ifTrue);
    std::vector<P4Scope *> then_state = state->get_state();
    state->restore_state(saved_state);
    if (not ifs->ifFalse) {
    } else {
        visit(ifs->ifFalse);
    }
    if (z3::expr *z3_cond = boost::get<z3::expr>(&cond)) {
        state->merge_state(!*z3_cond, state->get_state(), then_state);
    } else {
        BUG("Unsupported condition type.");
    }
    return false;
}

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (auto c : b->components) {
        visit(c);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::MethodCallStatement *mcs) {
    visit(mcs->methodCall);
    return false;
}

void Z3Visitor::set_var(const IR::Expression *target, P4Z3Instance val) {
    if (auto name = target->to<IR::PathExpression>()) {
        state->update_var(name->path->name, val);
    } else if (auto member = target->to<IR::Member>()) {
        visit(member->expr);
        P4Z3Instance complex_class = state->return_expr;
        StructInstance *si = check_complex<StructInstance>(complex_class);
        if (not si) {
            BUG("Can not cast to StructInstance.");
            std::cout << complex_class << "\n";
        }
        si->set_var(member->member.name, val);
    } else {
        BUG("Unknown target %s!", target->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    visit(as->right);
    set_var(as->left, state->return_expr);
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
