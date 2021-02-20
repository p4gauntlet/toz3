#include <cstdio>
#include <utility>

#include "complex_type.h"
#include "ir/ir-generated.h"
#include "lib/exceptions.h"
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
            merged_vec.push_back({param->name.name, state->return_expr});
        } else {
            auto arg_expr = state->gen_instance(param->name.name, param->type);
            merged_vec.push_back({param->name.name, arg_expr});
        }
        idx++;
    }

    return merged_vec;
}

bool Z3Visitor::preorder(const IR::P4Control *c) {
    // DO SOMETHING
    visit(c->body);
    return false;
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    visit(as->right);
    auto var = state->return_expr;
    auto target = as->left;
    if (auto name = target->to<IR::PathExpression>()) {
        state->insert_var(name->path->name, var);
    } else if (auto member = target->to<IR::Member>()) {
        visit(member->expr);
        P4Z3Instance complex_class = state->return_expr;
        StructInstance *si = check_complex<StructInstance>(state->return_expr);
        if (not si) {
            BUG("Unknown class");
            std::cout << complex_class << "\n";
        }
        si->members.at(member->member.name) = var;
    } else {
        BUG("Unknown target %s!", target->node_type_name());
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = state->resolve_type(di->type);
    if (auto pkt_type = resolved_type->to<IR::Type_Package>()) {
        decl_result =
            merge_args_with_params(di->arguments, pkt_type->getParameters());
    } else {
        BUG("Declaration Instance Type %s not supported.",
            resolved_type->node_type_name());
    }
    return false;
}

} // namespace TOZ3_V2
