#include <cstdio>
#include <iostream>
#include <utility>
#include <z3++.h>

#include "complex_type.h"
#include "ir/ir-generated.h"
#include "lib/exceptions.h"
#include "z3_int.h"
#include "z3_interpreter.h"

namespace TOZ3_V2 {

P4Z3Instance Z3Visitor::cast(P4Z3Instance expr, const IR::Type *dest_type) {
    if (auto tb = dest_type->to<IR::Type_Bits>()) {
        if (z3::expr *z3_var = boost::get<z3::expr>(&expr)) {
            if (z3_var->get_sort().is_bv()) {
                return state->ctx->bv_val(z3_var->get_decimal_string(0).c_str(),
                                          dest_type->width_bits());
            } else {
                BUG("Cast type not supported ");
            }
        } else if (auto z3_var = check_complex<Z3Int>(expr)) {
            auto val_string = z3_var->val.get_decimal_string(0);
            return state->ctx->bv_val(val_string.c_str(),
                                      dest_type->width_bits());
        } else {
            BUG("Cast from expr xpr to node %s supported ",
                dest_type->node_type_name());
        }
    } else {
        BUG("Cast to type %s not supported", dest_type->node_type_name());
    }
}

bool Z3Visitor::preorder(const IR::Equ *expr) {
    visit(expr->left);
    auto left = state->return_expr;
    visit(expr->right);
    auto right = state->return_expr;

    if (z3::expr *z3_left_var = boost::get<z3::expr>(&left)) {
        if (z3::expr *z3_right_var = boost::get<z3::expr>(&right)) {
            printf("YOOO\n");
            state->return_expr = *z3_left_var == *z3_right_var;
        } else {
            BUG("Z3 eq with int not yet supported. ");
        }
    } else if (auto z3_var = check_complex<Z3Int>(left)) {
        BUG("Int eq not supported. ");
    } else if (auto z3_left = check_complex<StructInstance>(left)) {
        BUG("Z3 Struct eq not yet supported. ");
    } else {
        BUG("Merge not supported. ");
    }
    return false;
}

bool Z3Visitor::preorder(const IR::Constant *c) {
    auto val_string = Util::toString(c->value, 0, false);
    if (auto tb = c->type->to<IR::Type_Bits>()) {
        if (tb->isSigned) {
            state->return_expr =
                new Z3Int(state->ctx->int_val(val_string), tb->size);
        } else {
            state->return_expr = state->ctx->bv_val(val_string, tb->size);
        }
        return false;
    } else if (c->type->is<IR::Type_InfInt>()) {
        state->return_expr = new Z3Int(state->ctx->int_val(val_string), -1);
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

bool Z3Visitor::preorder(const IR::Member *m) {
    P4Z3Instance complex_class = nullptr;
    const IR::Expression *parent = m->expr;
    if (auto member = parent->to<IR::Member>()) {
        visit(member);
        complex_class = state->return_expr;
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
    state->return_expr = si->members.at(m->member.name);
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

} // namespace TOZ3_V2
