#include "create_z3.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "exceptions.h"
#include "ir/id.h"
#include "ir/vector.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/null.h"
#include "lib/ordered_map.h"
#include "state.h"
#include "toz3/common/type_complex.h"
#include "toz3/common/type_simple.h"
#include "toz3/common/visitor_interpret.h"
#include "type_inference.h"
#include "visitor_specialize.h"
#include "z3++.h"

namespace P4::ToZ3 {

namespace {
std::vector<cstring> initializeParser(P4State *state, const IR::Type *resolved_type,
                                      const IR::ParameterList *params, cstring param_name) {
    std::vector<cstring> param_names;
    if (resolved_type->is<IR::P4Parser>()) {
        unsigned packet_inputs = 0;
        for (const auto *parameter : *params) {
            const auto *type = state->resolve_type(parameter->type)->to<IR::Type_Extern>();
            if ((type != nullptr) && type->name == "packet_in") {
                ++packet_inputs;
            }
        }
        if (packet_inputs > 1) {
            throw UnsupportedFeatureError("Parser with multiple independent packet inputs");
        }
        state->declare_var("$parser_accepted"_cs,
                           allocate_instance<Z3Bitvector>(state, state, &BOOL_TYPE,
                                                          state->get_z3_ctx()->bool_val(true)),
                           &BOOL_TYPE);
        param_names.push_back("$parser_accepted"_cs);
        const auto *error_bits = IR::Type_Bits::get(INT_WIDTH);
        state->declare_var("$parser_error"_cs,
                           allocate_instance<Z3Bitvector>(
                               state, state, error_bits, state->get_z3_ctx()->bv_val(0, INT_WIDTH)),
                           error_bits);
        param_names.push_back("$parser_error"_cs);
        state->declare_var("$packet_cursor"_cs, allocate_instance<Z3Int>(state, state, 0),
                           &INT_TYPE);
        state->declare_var(
            "$packet_id"_cs,
            allocate_instance<Z3Bitvector>(state, state, &STRING_TYPE,
                                           state->get_z3_ctx()->string_val(param_name.c_str())),
            &STRING_TYPE);
        param_names.push_back("$packet_cursor"_cs);
    }
    return param_names;
}
z3::expr zeroValue(const z3::expr &value) {
    auto &ctx = value.ctx();
    if (value.is_bool()) {
        return ctx.bool_val(false);
    }
    if (value.is_bv()) {
        return ctx.bv_val(0, value.get_sort().bv_size());
    }
    return value.is_int() ? ctx.int_val(0) : ctx.string_val("");
}

std::vector<std::pair<cstring, z3::expr>> collectOutputs(const P4State *state,
                                                         const std::vector<cstring> &names) {
    std::vector<std::pair<cstring, z3::expr>> outputs;
    for (const auto name : names) {
        const auto *value = state->get_var(name);
        if (const auto *numeric = value->to<NumericVal>()) {
            outputs.emplace_back(name, *numeric->get_val());
        } else if (const auto *aggregate = value->to<StructBase>()) {
            const auto fields = aggregate->get_z3_vars(name);
            outputs.insert(outputs.end(), fields.begin(), fields.end());
        } else if (!value->is<ExternInstance>()) {
            throw InternalError("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }
    return outputs;
}
}  // namespace

std::vector<std::pair<cstring, z3::expr>> run_arch_block(Z3Visitor *visitor,
                                                         const IR::Expression *block,
                                                         const IR::Type *param_type,
                                                         cstring param_name) {
    auto *state = visitor->get_state();
    visitor->visit(block);
    param_type = architecture_type(param_type);
    const auto *constructed_expr = state->get_expr_result()->cast_allocate(param_type);
    const auto *resolved_type = constructed_expr->get_p4_type();
    // Externs have no executable architecture block or observable output summary.
    if (constructed_expr->is<ExternInstance>()) {
        return {};
    }
    const auto *ctrl_instance = constructed_expr->to<ControlInstance>();
    if (ctrl_instance == nullptr) {
        throw UnsupportedFeatureError("Type Declaration " + resolved_type->toString() +
                                      " of type " + resolved_type->node_type_name() +
                                      " not supported.");
    }
    const auto *params = resolved_type->checkedTo<IR::IApply>()->getApplyParameters();
    const auto fun_call = ctrl_instance->get_function(mangle_name("apply"_cs, params->size()));
    state->push_scope();
    auto param_names = initializeParser(state, resolved_type, params, param_name);
    IR::Vector<IR::Argument> synthesized_args;
    for (const auto *param : *params) {
        const auto *par_type = state->resolve_type(param->type);
        cstring instance_name = param_name + "." + param->name.name;
        if (!par_type->is<IR::Type_Package>()) {
            auto *var = state->gen_instance(instance_name, par_type);
            if (param->direction != IR::Direction::Out) {
                if (auto *z3_var = var->to_mut<StructBase>()) {
                    z3_var->propagate_validity(nullptr);
                    z3_var->bind(nullptr, 0);
                }
            }
            state->declare_var(param->name.name, var, par_type);
        }
        param_names.push_back(param->name.name);
        const auto *arg = new IR::Argument(new IR::PathExpression(param->name.name));
        synthesized_args.push_back(arg);
    }
    // Call the apply function of the pipeline
    const auto enclosingTermination = state->get_termination_condition();
    state->set_termination_condition(state->get_z3_ctx()->bool_val(true));
    if (const auto *function = std::get_if<P4Z3Function>(&fun_call)) {
        (*function)(visitor, &synthesized_args);
    } else {
        throw InternalError("Unexpected main function.");
    }
    // Merge the exit states
    state->merge_exit_states();

    // COLLECT
    auto state_vars = collectOutputs(state, param_names);

    const auto termination = state->get_termination_condition().simplify();
    for (auto &variable : state_vars) {
        if (termination.is_true()) {
            continue;
        }
        auto &value = variable.second;
        value = z3::ite(termination, value, zeroValue(value));
    }
    state_vars.emplace_back("$terminated"_cs, termination);
    state->set_termination_condition(enclosingTermination);
    state->pop_scope();

    return state_vars;
}

namespace {
using ConstructorArguments = ordered_map<const IR::Parameter *, const IR::Expression *>;

ConstructorArguments mapConstructorArguments(const ParamInfo &param_info) {
    size_t idx = 0;

    ConstructorArguments param_mapping;
    for (const auto &param : param_info.params) {
        // This may have a nullptr, but we need to maintain order
        param_mapping.emplace(param, param->defaultValue);
    }
    for (const auto &arg : param_info.arguments) {
        // We override the mapping here.
        if (arg->name) {
            const auto *parameter = param_info.params.getParameter(arg->name.name);
            if (parameter == nullptr) {
                throw UnsupportedFeatureError("Unknown constructor argument " + arg->name.name);
            }
            param_mapping[parameter] = arg->expression;
        } else {
            param_mapping[param_info.params.getParameter(idx)] = arg->expression;
        }
        idx++;
    }
    return param_mapping;
}

TypeBindings inferArchitectureTypes(const P4State *state, const ParamInfo &param_info,
                                    const ConstructorArguments &param_mapping) {
    // Infer architecture types from supplied blocks before constructing defaults,
    // which may themselves use these type parameters.
    std::vector<TypeConstraint> constraints;
    for (const auto &mapping : param_mapping) {
        if ((mapping.second == nullptr) ||
            !contains_type_variables(mapping.first->type, param_info.type_params)) {
            continue;
        }
        // A default such as Empty<H>() consumes the inferred H; it supplies no
        // evidence until another constructor argument has determined that type.
        if (const auto *constructor = mapping.second->to<IR::ConstructorCallExpression>()) {
            if (contains_type_variables(constructor->constructedType, param_info.type_params)) {
                continue;
            }
        }
        constraints.push_back({mapping.first->type, expression_type(*state, mapping.second)});
    }
    return infer_type_arguments(*state, param_info.type_params, constraints);
}

void appendArchitectureArgument(Z3Visitor *visitor, const IR::Parameter *param,
                                const IR::Expression *arg_expr, MainResult *merged_vec) {
    auto *state = visitor->get_state();
    cstring param_name = param->name.name;
    const auto *param_type = param->type;
    // Ignore empty optional parameters, they can not be used properly
    if (param->isOptional() && arg_expr == nullptr) {
        return;
    }
    CHECK_NULL(arg_expr);
    if (const auto *cce = arg_expr->to<IR::ConstructorCallExpression>()) {
        if (visitor->get_state()->resolve_type(cce->constructedType)->is<IR::Type_Package>()) {
            const auto *instance = new IR::Declaration_Instance(
                IR::ID(param_name), cce->constructedType, cce->arguments);
            for (const auto &sub_result : gen_state_from_instance(visitor, instance)) {
                merged_vec->insert({param_name + sub_result.first, sub_result.second});
            }
            return;
        }
        auto state_result = run_arch_block(
            visitor, cce, visitor->get_state()->resolve_type(param_type), param_name);
        merged_vec->insert({param_name, {state_result, param_type}});
    } else if (const auto *path = arg_expr->to<IR::PathExpression>()) {
        const auto *decl = state->find_static_decl(path->path->name);
        if (decl == nullptr) {
            auto state_result =
                run_arch_block(visitor, path, state->resolve_type(param_type), param_name);
            merged_vec->insert({param_name, {state_result, param_type}});
            return;
        }
        const auto *di = decl->get_decl()->checkedTo<IR::Declaration_Instance>();
        auto sub_results = gen_state_from_instance(visitor, di);
        for (const auto &sub_result : sub_results) {
            auto merged_name = param_name + sub_result.first;
            auto variables = sub_result.second;
            merged_vec->insert({merged_name, variables});
        }
    } else if (const auto *cst = arg_expr->to<IR::Literal>()) {
        visitor->visit(cst);
        if (const auto *z3_val = visitor->get_state()->get_expr_result()->to<ValContainer>()) {
            merged_vec->insert(
                {param->name.name, {{{param->name.name, *z3_val->get_val()}}, param_type}});
        } else {
            throw UnsupportedFeatureError("Unsupported main argument " + arg_expr->toString() +
                                          " of type " + arg_expr->node_type_name());
        }
    } else {
        throw UnsupportedFeatureError("Unsupported main argument " + arg_expr->toString() +
                                      " of type " + arg_expr->node_type_name());
    }
}
}  // namespace

MainResult create_state(Z3Visitor *visitor, const ParamInfo &param_info) {
    MainResult merged_vec;
    const auto param_mapping = mapConstructorArguments(param_info);
    const auto inferred_types =
        inferArchitectureTypes(visitor->get_state(), param_info, param_mapping);
    const auto *concrete_params =
        param_info.params.apply(TypeModifier(&inferred_types))->checkedTo<IR::ParameterList>();
    for (const auto &mapping : param_mapping) {
        const auto *param = concrete_params->getParameter(mapping.first->name);
        const auto *arg_expr =
            mapping.second == mapping.first->defaultValue ? param->defaultValue : mapping.second;
        appendArchitectureArgument(visitor, param, arg_expr, &merged_vec);
    }
    return merged_vec;
}

MainResult gen_state_from_instance(Z3Visitor *visitor, const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = visitor->get_state()->resolve_type(di->type);
    const auto *container = resolved_type->to<IR::IContainer>();
    if (container == nullptr) {
        throw UnsupportedFeatureError("Callable declaration type " + resolved_type->toString() +
                                      " of type " + resolved_type->node_type_name() +
                                      " not supported.");
    }
    ParamInfo param_info{*container->getConstructorParameters(),
                         *di->arguments,
                         *container->getTypeParameters(),
                         {}};
    return create_state(visitor, param_info);
}

const IR::Declaration_Instance *get_main_decl(P4State *state) {
    const auto *main = state->find_static_decl("main"_cs);
    if (main != nullptr) {
        if (const auto *main_pkg = main->get_decl()->to<IR::Declaration_Instance>()) {
            return main_pkg;
        }
        throw UnsupportedFeatureError("Main node " + main->get_decl()->node_type_name() +
                                      " not implemented!");
    }
    warning("No main declaration found in program.");
    return nullptr;
}

}  // namespace P4::ToZ3
