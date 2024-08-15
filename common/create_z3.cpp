#include "create_z3.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/variant/get.hpp>

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
#include "visitor_specialize.h"
#include "z3++.h"

namespace P4::ToZ3 {

std::map<cstring, const IR::Type *> get_type_mapping_2(const IR::ParameterList *src_params,
                                                       const IR::TypeParameters *src_type_params,
                                                       const IR::ParameterList *dest_params) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto dest_params_size = dest_params->size();
    for (size_t idx = 0; idx < src_params->size(); ++idx) {
        // Ignore optional params
        if (idx >= dest_params_size) {
            continue;
        }
        const auto *src_param = src_params->getParameter(idx);
        const auto *dst_param = dest_params->getParameter(idx);
        if (const auto *tn = src_param->type->to<IR::Type_Name>()) {
            auto src_type_name = tn->path->name.name;
            if (src_type_params->getDeclByName(src_type_name) != nullptr) {
                type_mapping.emplace(src_type_name, dst_param->type);
            }
        }
    }
    return type_mapping;
}

std::map<cstring, const IR::Type *> specialize_arch_blocks(const IR::Type *src_type,
                                                           const IR::Type *dest_type) {
    if (const auto *control = src_type->to<IR::Type_Control>()) {
        if (const auto *control_dst_type = dest_type->to<IR::P4Control>()) {
            const auto *src_params = control->getApplyParameters();
            const auto *src_type_params = control->getTypeParameters();
            auto type_mapping = get_type_mapping_2(src_params, src_type_params,
                                                   control_dst_type->getApplyParameters());
            TypeModifier type_modifier(&type_mapping);
            return type_mapping;
        }
    }
    if (const auto *parser = src_type->to<IR::Type_Parser>()) {
        if (const auto *parser_dst_type = dest_type->to<IR::P4Parser>()) {
            const auto *src_params = parser->getApplyParameters();
            const auto *src_type_params = parser->getTypeParameters();
            auto type_mapping = get_type_mapping_2(src_params, src_type_params,
                                                   parser_dst_type->getApplyParameters());
            return type_mapping;
        }
    }
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s", src_type, dest_type);
}

std::vector<std::pair<cstring, z3::expr>> run_arch_block(Z3Visitor *visitor,
                                                         const IR::ConstructorCallExpression *cce,
                                                         const IR::Type *param_type,
                                                         cstring param_name,
                                                         const IR::TypeParameters &meta_params) {
    auto *state = visitor->get_state();

    visitor->visit(cce);
    const IR::ParameterList *params = nullptr;
    FunOrMethod fun_call = nullptr;
    // TODO: Refactor all of this into a separate pass
    const auto *constructed_expr = state->get_expr_result()->cast_allocate(param_type);
    const auto *resolved_type = constructed_expr->get_p4_type();
    if (const auto *ctrl_instance = constructed_expr->to<ControlInstance>()) {
        if (const auto *c = resolved_type->to<IR::P4Control>()) {
            auto type_mapping = specialize_arch_blocks(param_type, resolved_type);
            for (const auto &mapped_type : type_mapping) {
                if (meta_params.getDeclByName(mapped_type.first) != nullptr &&
                    visitor->get_state()->check_for_type(mapped_type.first) == nullptr) {
                    visitor->get_state()->add_type(mapped_type.first, mapped_type.second);
                }
            }
            params = c->getApplyParameters();
            cstring apply_name = "apply" + std::to_string(params->size());
            fun_call = ctrl_instance->get_function(apply_name);
        } else if (const auto *p = resolved_type->to<IR::P4Parser>()) {
            P4::warning("Ignoring parser output.");
            return {};
            auto type_mapping = specialize_arch_blocks(param_type, resolved_type);
            for (const auto &mapped_type : type_mapping) {
                if (meta_params.getDeclByName(mapped_type.first) != nullptr &&
                    visitor->get_state()->check_for_type(mapped_type.first) == nullptr) {
                    visitor->get_state()->add_type(mapped_type.first, mapped_type.second);
                }
            }
            params = p->getApplyParameters();
            cstring apply_name = "apply" + std::to_string(params->size());
            fun_call = ctrl_instance->get_function(apply_name);
        } else {
            P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.", resolved_type,
                              resolved_type->node_type_name());
        }
    } else if (constructed_expr->is<ExternInstance>()) {
        // Not sure what to do here yet...
        return {};
    } else {
        P4C_UNIMPLEMENTED("Type Declaration %s of type %s not supported.", resolved_type,
                          resolved_type->node_type_name());
    }
    // INITIALIZE
    // TODO: Simplify this
    state->push_scope();

    std::vector<cstring> param_names;
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
    if (const auto *function = boost::get<P4Z3Function>(&fun_call)) {
        (*function)(visitor, &synthesized_args);
    } else {
        BUG("Unexpected main function.");
    }
    // Merge the exit states
    state->merge_exit_states();

    // COLLECT
    std::vector<std::pair<cstring, z3::expr>> state_vars;
    for (auto param_name : param_names) {
        const auto *var = state->get_var(param_name);
        if (const auto *z3_var = var->to<NumericVal>()) {
            state_vars.emplace_back(param_name, *z3_var->get_val());
        } else if (const auto *z3_var = var->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(param_name);
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(), z3_sub_vars.end());
        } else if (var->is<ExternInstance>()) {
            // warning("Skipping extern because we do not know how to represent
            // "it.");
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }

    state->pop_scope();

    return state_vars;
}

MainResult create_state(Z3Visitor *visitor, const ParamInfo &param_info) {
    MainResult merged_vec;
    size_t idx = 0;

    ordered_map<const IR::Parameter *, const IR::Expression *> param_mapping;
    for (const auto &param : param_info.params) {
        // This may have a nullptr, but we need to maintain order
        param_mapping.emplace(param, param->defaultValue);
    }
    for (const auto &arg : param_info.arguments) {
        // We override the mapping here.
        if (arg->name) {
            param_mapping[param_info.params.getParameter(arg->name.name)] = arg->expression;
        } else {
            param_mapping[param_info.params.getParameter(idx)] = arg->expression;
        }
        idx++;
    }

    for (const auto &mapping : param_mapping) {
        const auto *param = mapping.first;
        cstring param_name = param->name.name;
        const auto *param_type = param->type;
        const auto *arg_expr = mapping.second;
        // Ignore empty optional parameters, they can not be used properly
        if (param->isOptional() && arg_expr == nullptr) {
            continue;
        }
        CHECK_NULL(arg_expr);
        if (const auto *cce = arg_expr->to<IR::ConstructorCallExpression>()) {
            auto state_result =
                run_arch_block(visitor, cce, visitor->get_state()->resolve_type(param_type),
                               param_name, param_info.type_params);
            merged_vec.insert({param_name, {state_result, param_type}});
        } else if (const auto *path = arg_expr->to<IR::PathExpression>()) {
            const auto *decl = visitor->get_state()->get_static_decl(path->path->name.name);
            const auto *di = decl->get_decl()->checkedTo<IR::Declaration_Instance>();
            auto sub_results = gen_state_from_instance(visitor, di);
            for (const auto &sub_result : sub_results) {
                auto merged_name = param_name + sub_result.first;
                auto variables = sub_result.second;
                merged_vec.insert({merged_name, variables});
            }
        } else if (const auto *cst = arg_expr->to<IR::Literal>()) {
            visitor->visit(cst);
            if (const auto *z3_val = visitor->get_state()->get_expr_result()->to<ValContainer>()) {
                merged_vec.insert(
                    {param->name.name, {{{param->name.name, *z3_val->get_val()}}, param_type}});
            } else {
                P4C_UNIMPLEMENTED("Unsupported main argument %s of type %s", arg_expr,
                                  arg_expr->node_type_name());
            }
        } else {
            P4C_UNIMPLEMENTED("Unsupported main argument %s of type %s", arg_expr,
                              arg_expr->node_type_name());
        }
    }
    return merged_vec;
}

MainResult gen_state_from_instance(Z3Visitor *visitor, const IR::Declaration_Instance *di) {
    const IR::Type *resolved_type = visitor->get_state()->resolve_type(di->type);
    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    if (const auto *control = resolved_type->to<IR::P4Control>()) {
        params = control->getConstructorParameters();
        type_params = control->getTypeParameters();
    } else if (const auto *parser = resolved_type->to<IR::P4Parser>()) {
        params = parser->getConstructorParameters();
        type_params = parser->getTypeParameters();
    } else if (const auto *package = resolved_type->to<IR::Type_Package>()) {
        params = package->getConstructorParameters();
        type_params = package->getTypeParameters();
    } else {
        P4C_UNIMPLEMENTED("Callable declaration type %s of type %s not supported.", resolved_type,
                          resolved_type->node_type_name());
    }
    ParamInfo param_info{*params, *di->arguments, *type_params, {}};
    return create_state(visitor, param_info);
}

const IR::Declaration_Instance *get_main_decl(P4State *state) {
    const auto *main = state->find_static_decl("main"_cs);
    if (main != nullptr) {
        if (const auto *main_pkg = main->get_decl()->to<IR::Declaration_Instance>()) {
            return main_pkg;
        }
        P4C_UNIMPLEMENTED("Main node %s not implemented!", main->get_decl()->node_type_name());
    }
    warning("No main declaration found in program.");
    return nullptr;
}

}  // namespace P4::ToZ3
