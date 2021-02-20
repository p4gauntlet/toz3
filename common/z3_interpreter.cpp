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

bool Z3Visitor::preorder(const IR::P4Control *c) {
    // DO SOMETHING
    visit(c->body);
    return false;
}

P4Z3Instance Z3Visitor::cast(P4Z3Instance expr, const IR::Type *dest_type) {
    if (auto tb = dest_type->to<IR::Type_Bits>()) {
        if (z3::expr *z3_var = boost::get<z3::expr>(&expr)) {
            if (z3_var->get_sort().is_bv()) {
                return state->ctx->bv_val(z3_var->get_decimal_string(0).c_str(),
                                          dest_type->width_bits());
            } else if (z3_var->get_sort().is_bool()) {

            } else {
            }

        } else if (auto z3_var = check_complex<Z3Int>(expr)) {
            auto val_string = Util::toString(z3_var->val, 0, false);
            return state->ctx->bv_val(val_string, dest_type->width_bits());
        } else {
            BUG("Cast from expr xpr to node %s supported ",
                dest_type->node_type_name());
        }
    } else {
        BUG("Cast to type %s not supported", dest_type->node_type_name());
    }
}

P4Z3Instance Z3Visitor::resolve_member(const IR::Member *m) {
    P4Z3Instance complex_class = nullptr;
    const IR::Expression *parent = m->expr;
    if (auto member = parent->to<IR::Member>()) {
        complex_class = resolve_member(member);
    } else if (auto name = parent->to<IR::PathExpression>()) {
        complex_class = state->get_var(name->path->name);
    } else {
        BUG("Parent Type  %s not implemented!", parent->node_type_name());
    }
    StructInstance *si = check_complex<StructInstance>(complex_class);
    if (not si) {
        BUG("Unknown class");
        std::cout << complex_class << "\n";
    }
    return si->members.at(m->member.name);
}

bool Z3Visitor::preorder(const IR::BlockStatement *b) {
    for (auto c : b->components) {
        visit(c);
    }
    return false;
}

bool Z3Visitor::preorder(const IR::AssignmentStatement *as) {
    visit(as->right);
    auto var = state->return_expr;
    auto target = as->left;
    if (auto name = target->to<IR::PathExpression>()) {
        state->insert_var(name->path->name, var);
    } else if (auto member = target->to<IR::Member>()) {
        const IR::Expression *parent = member->expr;
        P4Z3Instance complex_class = nullptr;
        if (auto sub_member = parent->to<IR::Member>()) {
            complex_class = resolve_member(sub_member);
        } else if (auto name = parent->to<IR::PathExpression>()) {
            complex_class = state->get_var(name->path->name);
        } else {
            BUG("Parent Type  %s not implemented!", parent->node_type_name());
        }
        StructInstance *si = check_complex<StructInstance>(complex_class);
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

bool Z3Visitor::preorder(const IR::Constant *c) {
    auto val_string = Util::toString(c->value, 0, false);
    if (auto tb = c->type->to<IR::Type_Bits>()) {
        if (tb->isSigned) {
            state->return_expr = new Z3Int(c->value, tb->size);
        } else {
            state->return_expr = state->ctx->bv_val(val_string, tb->size);
        }
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        state->return_expr = new Z3Int(c->value, -1);
        return false;
    }
    BUG("Constant Node %s not implemented!", c->type->node_type_name());
}

bool Z3Visitor::preorder(const IR::PathExpression *p) {
    P4Scope *scope;
    state->return_expr = state->find_var(p->path->name, &scope);
    return false;
}

bool Z3Visitor::preorder(const IR::Cast *c) {
    // resolve expression
    visit(c->expr);
    auto resolved_expr = state->return_expr;
    state->return_expr = cast(resolved_expr, c->destType);

    return false;
}

bool Z3Visitor::preorder(const IR::ConstructorCallExpression *cce) {
    const IR::Type *resolved_type = state->resolve_type(cce->constructedType);
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    if (auto c = resolved_type->to<IR::P4Control>()) {
        auto scope = new P4Scope();
        state->add_scope(scope);
        std::vector<cstring> state_names;
        // INITIALIZE
        for (auto param : *c->getApplyParameters()) {
            auto par_type = state->resolve_type(param->type);
            P4Z3Instance var = state->gen_instance(param->name.name, par_type);
            state->insert_var(param->name.name, var);
            state_names.push_back(param->name.name);
        }

        // VISIT THE CONTROL
        visit(resolved_type);

        // COLLECT
        for (auto state_name : state_names) {
            P4Scope *scope;
            auto member = state->find_var(state_name, &scope);
            if (z3::expr *z3_var = boost::get<z3::expr>(&member)) {
                state_vars.push_back({state_name, *z3_var});
            } else if (auto z3_var = check_complex<StructInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (auto z3_var = check_complex<ErrorInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (auto z3_var = check_complex<EnumInstance>(member)) {
                auto z3_sub_vars = z3_var->get_z3_vars(state_name);
                state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                                  z3_sub_vars.end());
            } else if (check_complex<ExternInstance>(member)) {
                printf("Skipping extern...\n");
            } else {
                BUG("Var is neither type z3::expr nor P4ComplexInstance!");
            }
        }
    }
    state->return_expr = new ControlState(state_vars);

    return false;
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
