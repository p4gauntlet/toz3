#include "state.h"

#include <z3++.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/reverse_iterator.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/et_ops.hpp>
#include <boost/multiprecision/number.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "evaluation_context.h"
#include "ir/id.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/null.h"
#include "lib/ordered_map.h"
#include "lib/stringify.h"
#include "toz3/common/scope.h"
#include "toz3/common/util.h"
#include "type_base.h"
#include "type_inference.h"
#include "visitor_specialize.h"

namespace P4::ToZ3 {

z3::expr compute_slice(const z3::expr &lval, const z3::expr &rval,
                       const std::vector<Z3Slice> &end_slices) {
    auto *ctx = &lval.get_sort().ctx();
    auto lval_max = lval.get_sort().bv_size() - 1ULL;
    auto lval_min = 0ULL;
    auto slice_l = lval_max;
    auto slice_r = 0ULL;
    for (auto sl = end_slices.rbegin(); sl != end_slices.rend(); ++sl) {
        auto hi_int = sl->hi.simplify().get_numeral_uint64();
        auto lo_int = sl->lo.simplify().get_numeral_uint64();
        slice_l = hi_int + slice_r;
        slice_r += lo_int;
    }
    if (slice_l == lval_max && slice_r == lval_min) {
        return rval;
    }

    z3::expr_vector assemble(*ctx);
    if (slice_l < lval_max) {
        assemble.push_back(lval.extract(lval_max, slice_l + 1));
    }
    auto middle_size = ctx->bv_sort(slice_l + 1 - slice_r);
    auto cast_val = pure_bv_cast(rval, middle_size);
    assemble.push_back(cast_val);

    if (slice_r > lval_min) {
        assemble.push_back(lval.extract(slice_r - 1, lval_min));
    }

    return z3::concat(assemble);
}

MemberStruct get_member_struct(P4State *state, EvaluationContext *visitor,
                               const IR::Expression *target) {
    MemberStruct member_struct;
    member_struct.is_flat = true;
    const auto append = [&](const NameOrIndex &member) {
        if (!member_struct.is_flat) {
            member_struct.mid_members.insert(member_struct.mid_members.begin(),
                                             member_struct.target_member);
        }
        member_struct.target_member = member;
        member_struct.is_flat = false;
    };
    std::function<void(const IR::Expression *)> resolve = [&](const IR::Expression *expression) {
        if (const auto *member = expression->to<IR::Member>()) {
            resolve(member->expr);
            append(member->member.name);
        } else if (const auto *index = expression->to<IR::ArrayIndex>()) {
            resolve(index->left);
            visitor->evaluate(index->right);
            const auto value = state->get_expr_result<ValContainer>()->get_val()->simplify();
            append(value);
            member_struct.has_index = true;
        } else if (const auto *slice = expression->to<IR::Slice>()) {
            resolve(slice->e0);
            visitor->evaluate(slice->e1);
            const auto hi = *state->get_expr_result<NumericVal>()->get_val();
            visitor->evaluate(slice->e2);
            const auto lo = *state->get_expr_result<NumericVal>()->get_val();
            member_struct.end_slices.insert(member_struct.end_slices.begin(), {hi, lo});
        } else if (const auto *path = expression->to<IR::PathExpression>()) {
            member_struct.main_member = path->path->name;
        } else if (const auto *name = expression->to<IR::TypeNameExpression>()) {
            member_struct.main_member = name->typeName->checkedTo<IR::Type_Name>()->path->name;
        } else {
            visitor->evaluate(expression);
            member_struct.temporary = state->copy_expr_result();
        }
    };
    resolve(target);
    return member_struct;
}

std::vector<std::pair<z3::expr, P4Z3Instance *>> get_hdr_pairs(P4State *state,
                                                               const MemberStruct &member_struct) {
    std::vector<std::pair<z3::expr, P4Z3Instance *>> parent_pairs;
    auto tmp_parent_pairs = parent_pairs;
    parent_pairs.emplace_back(state->get_z3_ctx()->bool_val(true),
                              member_struct.temporary ? member_struct.temporary
                                                      : state->get_var(member_struct.main_member));
    // Collect all the headers that need to be set
    for (auto it = member_struct.mid_members.rbegin(); it != member_struct.mid_members.rend();
         ++it) {
        auto mid_member = *it;
        if (const auto *name = std::get_if<cstring>(&mid_member)) {
            for (auto &parent_pair : parent_pairs) {
                auto parent_cond = parent_pair.first;
                const auto *parent_class = parent_pair.second;
                tmp_parent_pairs.emplace_back(parent_cond, parent_class->get_member(*name));
            }
            parent_pairs = tmp_parent_pairs;
            tmp_parent_pairs.clear();
        } else if (const auto *expr = std::get_if<z3::expr>(&mid_member)) {
            for (auto &parent_pair : parent_pairs) {
                auto parent_cond = parent_pair.first;
                auto *parent_class = parent_pair.second;
                std::string val_str;
                if (expr->is_numeral(val_str, 0)) {
                    tmp_parent_pairs.emplace_back(parent_cond, parent_class->get_member(val_str));
                } else {
                    auto *stack_class = parent_class->to_mut<StackInstance>();
                    BUG_CHECK(stack_class, "Expected Stack, got %s",
                              parent_class->get_static_type());
                    // Skip anything where the idx is larger then the container
                    auto size = stack_class->get_int_size();
                    auto bv_size = expr->get_sort().bv_size();
                    big_int max = (big_int(1) << bv_size);
                    auto max_idx = std::min<big_int>(max, size);
                    for (big_int idx = 0; idx < max_idx; ++idx) {
                        auto member_name = Util::toString(idx, 0, false);
                        auto z3_val = state->get_z3_ctx()->bv_val(member_name.c_str(), bv_size);
                        tmp_parent_pairs.emplace_back(parent_cond && *expr == z3_val,
                                                      parent_class->get_member(member_name));
                    }
                }
            }
            parent_pairs = tmp_parent_pairs;
            tmp_parent_pairs.clear();
        } else {
            P4C_UNIMPLEMENTED("Member type not implemented.");
        }
    }
    return parent_pairs;
}

void set_stack(P4State *state, const MemberStruct &member_struct, P4Z3Instance *rval) {
    auto parent_pairs = get_hdr_pairs(state, member_struct);

    // Set the variable
    if (const auto *name = std::get_if<cstring>(&member_struct.target_member)) {
        for (auto &parent_pair : parent_pairs) {
            auto parent_cond = parent_pair.first;
            auto *parent_class = parent_pair.second;
            auto *complex_class = parent_class->to_mut<StructBase>();
            CHECK_NULL(complex_class);
            const auto *orig_val = complex_class->get_member(*name);
            const auto *dest_type = complex_class->get_member_type(*name);
            auto *cast_val = rval->cast_allocate(dest_type);
            cast_val->merge(!parent_cond, *orig_val);
            complex_class->update_member(*name, cast_val);
        }
    } else if (const auto *expr = std::get_if<z3::expr>(&member_struct.target_member)) {
        std::string val_str;
        for (auto &parent_pair : parent_pairs) {
            auto parent_cond = parent_pair.first;
            auto *parent_class = parent_pair.second;
            auto *complex_class = parent_class->to_mut<StructBase>();
            CHECK_NULL(complex_class);
            if (expr->is_numeral(val_str, 0)) {
                const auto *orig_val = complex_class->get_member(val_str);
                const auto *dest_type = complex_class->get_member_type(val_str);
                auto *cast_val = rval->cast_allocate(dest_type);
                cast_val->merge(!parent_cond, *orig_val);
                complex_class->update_member(val_str, cast_val);
            } else {
                auto *stack_class = complex_class->to_mut<StackInstance>();
                BUG_CHECK(stack_class, "Expected Stack, got %s", complex_class->get_static_type());
                // Skip anything where the idx is larger then the container
                auto size = stack_class->get_int_size();
                auto bv_size = expr->get_sort().bv_size();
                big_int max = (big_int(1) << bv_size);
                auto max_idx = std::min<big_int>(max, size);
                for (big_int idx = 0; idx < max_idx; ++idx) {
                    auto member_name = Util::toString(idx, 0, false);
                    auto *orig_val = complex_class->get_member(member_name);
                    const auto *dest_type = complex_class->get_member_type(member_name);
                    auto *cast_val = rval->cast_allocate(dest_type);
                    auto z3_val = state->get_z3_ctx()->bv_val(member_name.c_str(), bv_size);
                    cast_val->merge(!(parent_cond && *expr == z3_val), *orig_val);
                    complex_class->update_member(member_name, cast_val);
                }
            }
        }
    } else {
        P4C_UNIMPLEMENTED("Member type not implemented.");
    }
}

P4Z3Instance *get_member(P4State *state, const MemberStruct &member_struct) {
    // TODO: Clarify this.
    auto *parent_class = member_struct.temporary ? member_struct.temporary
                                                 : state->get_var(member_struct.main_member);
    P4Z3Instance *end_var = nullptr;
    if (member_struct.is_flat) {
        // This means we are essentially dealing with a path expression.
        // They may have slices attached to it, so we can not just return.
        end_var = parent_class;
    } else {
        for (auto it = member_struct.mid_members.rbegin(); it != member_struct.mid_members.rend();
             ++it) {
            auto mid_member = *it;
            if (const auto *name = std::get_if<cstring>(&mid_member)) {
                parent_class = parent_class->get_member(*name);
            } else if (const auto *z3_expr = std::get_if<z3::expr>(&mid_member)) {
                std::string index;
                if (z3_expr->is_numeral(index)) {
                    parent_class = parent_class->get_member(index);
                } else {
                    const auto *array = parent_class->to<IndexableInstance>();
                    BUG_CHECK(array, "Expected indexable value");
                    parent_class = array->get_member(*z3_expr);
                }
            } else {
                P4C_UNIMPLEMENTED("Member type not implemented.");
            }
        }

        if (const auto *name = std::get_if<cstring>(&member_struct.target_member)) {
            end_var = parent_class->get_member(*name);
        } else if (const auto *z3_expr = std::get_if<z3::expr>(&member_struct.target_member)) {
            std::string index;
            if (z3_expr->is_numeral(index)) {
                end_var = parent_class->get_member(index);
            } else {
                const auto *array = parent_class->to<IndexableInstance>();
                BUG_CHECK(array, "Expected indexable value");
                end_var = array->get_member(*z3_expr);
            }
        } else {
            P4C_UNIMPLEMENTED("Member type not implemented.");
        }
    }

    // Finally, the member structure may also have slices attached to it.
    if (!member_struct.end_slices.empty()) {
        for (auto sl = member_struct.end_slices.rbegin(); sl != member_struct.end_slices.rend();
             ++sl) {
            end_var = end_var->slice(sl->hi, sl->lo);
        }
    }
    return end_var;
}

void P4State::set_var(const MemberStruct &member_struct, P4Z3Instance *rval) {
    if (!member_struct.end_slices.empty()) {
        auto end_slices = member_struct.end_slices;
        auto slice_less_member_struct = member_struct;
        slice_less_member_struct.end_slices.clear();
        z3::expr target_rval(*get_z3_ctx());
        z3::expr target_lval(*get_z3_ctx());
        const auto *lval = get_member(this, slice_less_member_struct);
        if (const auto *z3_bitvec = lval->to<Z3Bitvector>()) {
            target_lval = *z3_bitvec->get_val();
        } else {
            P4C_UNIMPLEMENTED("Unsupported lval of type %s for slice.", lval->get_static_type());
        }
        bool is_signed = false;
        if (const auto *z3_bitvec = rval->to<Z3Bitvector>()) {
            target_rval = *z3_bitvec->get_val();
            is_signed = z3_bitvec->bv_is_signed();
        } else if (const auto *z3_int = rval->to<Z3Int>()) {
            target_rval = *z3_int->get_val();
        } else {
            P4C_UNIMPLEMENTED("Unsupported rval of type %s for slice.", rval->get_static_type());
        }
        // We progressively slice and merge the lval
        target_rval = compute_slice(target_lval, target_rval, member_struct.end_slices);
        const auto *bit_type = IR::Type_Bits::get(target_rval.get_sort().bv_size(), false);
        auto *resolved_rval = new Z3Bitvector(this, bit_type, target_rval, is_signed);
        set_var(slice_less_member_struct, resolved_rval);
        return;
    }
    if (member_struct.is_flat) {
        // Flat target, just update state
        update_var(member_struct.main_member, rval);
        return;
    }
    // If we are dealing with a stack, start with a complicated procedure
    // We need to do this to resolve symbolic indices
    if (member_struct.has_index) {
        set_stack(this, member_struct, rval);
        return;
    }
    // This is the default mode where we only have strings for a member.
    auto *parent_class = get_var(member_struct.main_member);
    for (auto it = member_struct.mid_members.rbegin(); it != member_struct.mid_members.rend();
         ++it) {
        auto name = std::get<cstring>(*it);
        parent_class = parent_class->get_member(name);
    }
    if (const auto *name = std::get_if<cstring>(&member_struct.target_member)) {
        auto *complex_class = parent_class->to_mut<StructBase>();
        CHECK_NULL(complex_class);
        const auto *dest_type = complex_class->get_member_type(*name);
        auto *cast_val = rval->cast_allocate(dest_type);
        complex_class->update_member(*name, cast_val);
    }
}

void P4State::set_var(EvaluationContext *visitor, const IR::Expression *target,
                      P4Z3Instance *rval) {
    if (const auto *name = target->to<IR::PathExpression>()) {
        const auto *dest_type = get_var_type(name->path->name.name);
        auto *cast_val = rval->cast_allocate(dest_type);
        update_var(name->path->name, cast_val);
        return;
    }
    auto member_struct = get_member_struct(this, visitor, target);
    // Collection phase done
    // Now begins the setting phase...
    set_var(member_struct, rval);
}

void P4State::set_var(EvaluationContext *visitor, const IR::Expression *target,
                      const IR::Expression *rval) {
    if (const auto *name = target->to<IR::PathExpression>()) {
        const auto *dest_type = get_var_type(name->path->name.name);
        visitor->evaluate(rval);
        const auto *tmp_rval = get_expr_result();
        auto *cast_val = tmp_rval->cast_allocate(dest_type);
        update_var(name->path->name, cast_val);
        return;
    }
    auto member_struct = get_member_struct(this, visitor, target);
    // Collection phase done
    // Now begins the setting phase...
    visitor->evaluate(rval);
    auto *tmp_rval = copy_expr_result();
    set_var(member_struct, tmp_rval);
}

std::pair<CopyArgs, VarMap> P4State::merge_args_with_params(EvaluationContext *visitor,
                                                            const IR::Vector<IR::Argument> &args,
                                                            const IR::ParameterList &params,
                                                            const IR::TypeParameters &type_params) {
    CopyArgs resolved_args;
    VarMap merged_vec;
    // Save inputs and copy-back destinations in call-site order, including named arguments.
    ordered_map<const IR::Parameter *, const IR::Expression *> param_mapping;
    for (size_t idx = 0; idx < args.size(); ++idx) {
        const auto *arg = args.at(idx);
        const auto *param =
            arg->name ? params.getParameter(arg->name.name) : params.getParameter(idx);
        BUG_CHECK(param && !param_mapping.count(param), "Invalid argument %s", arg);
        param_mapping.emplace(param, arg->expression);
    }
    for (const auto *param : params) {
        if (!param_mapping.count(param)) param_mapping.emplace(param, param->defaultValue);
    }

    for (const auto &mapping : param_mapping) {
        const auto *param = mapping.first;
        const auto *arg_expr = mapping.second;
        // Ignore empty optional parameters, they can not be used properly
        if (param->isOptional() && arg_expr == nullptr) {
            continue;
        }
        CHECK_NULL(arg_expr);
        // If the expression is default, we can not save a copy
        if (arg_expr->is<IR::DefaultExpression>()) {
            BUG_CHECK(param->direction == IR::Direction::Out, "Discarded argument must be out");
            const auto *type = resolve_type(param->type);
            merged_vec.emplace(param->name.name,
                               std::make_pair(gen_instance(cstring(UNDEF_LABEL), type), type));
            continue;
        }

        const P4Z3Instance *arg_result = nullptr;
        auto direction = param->direction;
        if (direction == IR::Direction::Out || direction == IR::Direction::InOut) {
            auto member_struct = get_member_struct(this, visitor, arg_expr);
            resolved_args.push_back({member_struct, param->name.name});
            arg_result = get_member(this, member_struct);
        } else {
            visitor->evaluate(arg_expr);
            arg_result = get_expr_result();
        }
        CHECK_NULL(arg_result);
        if (const auto *tn = param->type->to<IR::Type_Name>()) {
            cstring type_name = tn->path->name.name;
            if (type_params.getDeclByName(type_name) != nullptr) {
                // Need to infer a type here and add it to the scope
                // TODO: This should be a separate pass.
                add_type(type_name, arg_result->get_p4_type());
            }
        }
        const auto *resolved_type = resolve_type(param->type);
        if (direction == IR::Direction::Out) {
            auto *instance = gen_instance(cstring(UNDEF_LABEL), resolved_type);
            merged_vec.insert({param->name.name, {instance, resolved_type}});
        } else {
            P4Z3Instance *cast_val = nullptr;
            // If the type is don't care we just copy the input argument.
            if (resolved_type->is<IR::Type_Dontcare>()) {
                cast_val = arg_result->copy();
            } else {
                cast_val = arg_result->cast_allocate(resolved_type);
            }
            merged_vec.insert({param->name.name, {cast_val, resolved_type}});
        }
    }
    return std::pair<CopyArgs, VarMap>{resolved_args, merged_vec};
}

void P4State::copy_in(EvaluationContext *visitor, const ParamInfo &param_info) {
    push_scope();

    // Specialize
    size_t idx = 0;
    auto type_args_len = param_info.type_args.size();
    IR::TypeParameters missing_type_params;
    for (const auto &param : param_info.type_params.parameters) {
        if (idx >= type_args_len) {
            missing_type_params.push_back(param);
        }
        idx++;
    }
    auto var_tuple = merge_args_with_params(visitor, param_info.arguments, param_info.params,
                                            missing_type_params);
    auto copy_out_args = var_tuple.first;
    auto merged_vec = var_tuple.second;
    // Now we actually set the variables.
    // After we have resolved and collected them.
    for (auto arg_tuple : merged_vec) {
        cstring param_name = arg_tuple.first;
        auto arg_val = arg_tuple.second;
        declare_var(param_name, arg_val.first, arg_val.second);
    }
    set_copy_out_args(copy_out_args);
}

void P4State::copy_out() {
    auto copy_out_args = get_copy_out_args();
    // merge all the state of the different return points
    auto return_states = get_return_states();
    for (auto it = return_states.rbegin(); it != return_states.rend(); ++it) {
        merge_vars(it->first, it->second);
    }

    std::vector<P4Z3Instance *> copy_out_vals;
    for (const auto &arg_tuple : copy_out_args) {
        auto source = arg_tuple.second;
        auto *val = get_var(source);
        copy_out_vals.push_back(val);
    }

    pop_scope();
    size_t idx = 0;
    for (auto &arg_tuple : copy_out_args) {
        auto target = arg_tuple.first;
        set_var(target, copy_out_vals[idx]);
        idx++;
    }
}

z3::expr P4State::gen_z3_expr(cstring name, const IR::Type *type) {
    if (const auto *tbi = type->to<IR::Type_Bits>()) {
        if (tbi->size == 0) return ctx->int_val(0);
        return ctx->bv_const(name.c_str(), tbi->size);
    }
    if (const auto *tvb = type->to<IR::Type_Varbits>()) {
        return ctx->bv_const(name.c_str(), tvb->size);
    }
    if (type->is<IR::Type_Boolean>()) {
        return ctx->bool_const(name.c_str());
    }
    if (type->is<IR::Type_MatchKind>()) return ctx->bv_const(name.c_str(), 32);
    if (type->is<IR::Type_InfInt>()) return ctx->int_const(name.c_str());
    if (type->is<IR::Type_String>()) return ctx->constant(name.c_str(), ctx->string_sort());
    BUG("Type \"%v\" not supported for Z3 expressions!.", type);
}

P4Z3Instance *P4State::gen_instance(cstring name, const IR::Type *type, uint64_t id) {
    P4Z3Instance *instance = nullptr;
    if (const auto *tn = type->to<IR::Type_Name>()) {
        type = resolve_type(tn);
    }
    // TODO: Split this up to not muddle things.
    if (const auto *t = type->to<IR::Type_Struct>()) {
        instance = new StructInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_Header>()) {
        instance = new HeaderInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_Enum>()) {
        // TODO: Clean this up
        // For Enums we just return a copy of the declaration
        auto *enum_instance = get_var<EnumInstance>(t->name.name)->copy();
        CHECK_NULL(enum_instance);
        enum_instance->set_enum_val(gen_z3_expr(name, &P4_STD_BIT_TYPE));
        instance = enum_instance;
    } else if (const auto *t = type->to<IR::Type_Error>()) {
        // For Errors we just return a copy of the declaration
        auto *enum_instance = get_var<ErrorInstance>(t->name.name)->copy();
        CHECK_NULL(enum_instance);
        enum_instance->set_enum_val(gen_z3_expr(name, &P4_STD_BIT_TYPE));
        instance = enum_instance;
    } else if (const auto *t = type->to<IR::Type_SerEnum>()) {
        // For SerEnums we just return a copy of the declaration
        auto *enum_instance = get_var<SerEnumInstance>(t->name.name)->copy();
        CHECK_NULL(enum_instance);
        enum_instance->set_enum_val(gen_z3_expr(name, resolve_type(t->type)));
        instance = enum_instance;
    } else if (const auto *t = type->to<IR::Type_Array>()) {
        instance = new StackInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_HeaderUnion>()) {
        instance = new HeaderUnionInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_List>()) {
        instance = new ListInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_Tuple>()) {
        instance = new TupleInstance(this, t, name, id);
    } else if (const auto *t = type->to<IR::Type_Extern>()) {
        instance = new ExternInstance(this, t);
    } else if (type->is<IR::Type_Void>()) {
        instance = new VoidResult();
    } else if (type->is<IR::Type_InfInt>()) {
        instance = new Z3Int(this, gen_z3_expr(name, type));
    } else if (type->is<IR::Type_Base>()) {
        const auto *bits = type->to<IR::Type_Bits>();
        instance = new Z3Bitvector(this, type, gen_z3_expr(name, type), bits && bits->isSigned);
    } else {
        P4C_UNIMPLEMENTED("Instance generation for %s of type \"%s\" not supported!.", type,
                          type->node_type_name());
    }
    return instance;
}

void P4State::pop_lexical_scope() {
    const auto scope = get_current_scope();
    pop_scope();
    for (const auto &entry : scope.get_return_exprs()) push_return_expr(entry.first, entry.second);
    for (auto entry : scope.get_return_states()) {
        for (const auto &local : scope.get_var_map()) entry.second.erase(local.first);
        push_return_state(entry.first, entry.second);
    }
    for (const auto &condition : scope.get_return_conds()) push_return_cond(condition);
    if (scope.has_returned()) set_returned(true);
}

void P4State::push_scope() { scopes.push_back(P4Scope()); }

void P4State::pop_scope() { scopes.pop_back(); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    if (check_for_type(type_name) != nullptr) {
        warning("Type %s shadows existing type in target scope.", type_name);
    }
    if (scopes.empty()) {
        main_scope.add_type(type_name, t);
        // assume we insert into the global scope
    } else {
        get_mut_current_scope()->add_type(type_name, t);
    }
}

const IR::Type *P4State::get_type(cstring type_name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_type(type_name)) {
            return scope.get_type(type_name);
        }
    }
    // also check the parent scope
    return main_scope.get_type(type_name);
}

const IR::Type *P4State::resolve_type(const IR::Type *type) const {
    if (const auto *tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        type = get_type(type_name);
    }
    if (const auto *ts = type->to<IR::Type_Specialized>()) {
        TypeSpecializer specializer(*this, *ts->arguments);
        const auto *resolved_node = ts->baseType->apply(specializer);
        return resolved_node->checkedTo<IR::Type>();
    }
    return type;
}

const IR::Type *P4State::check_for_type(cstring type_name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_type(type_name)) {
            return scope.get_type(type_name);
        }
    }
    // also check the parent scope
    if (main_scope.has_type(type_name)) {
        return main_scope.get_type(type_name);
    }
    return nullptr;
}

const IR::Type *P4State::check_for_type(const IR::Type *t) const {
    if (const auto *tn = t->to<IR::Type_Name>()) {
        return check_for_type(tn->path->name.name);
    }
    return t;
}

P4Z3Instance *P4State::get_var(cstring name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_var(name)) {
            return scope.get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        return main_scope.get_var(name);
    }
    error("Variable %s not found in scope.", name);
    exit(1);
}

const IR::Type *P4State::get_var_type(cstring name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_var(name)) {
            return scope.get_var_type(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        return main_scope.get_var_type(name);
    }
    error("Variable %s not found in scope.", name);
    exit(1);
}

P4Z3Instance *P4State::find_var(cstring name, P4Scope **owner_scope) {
    for (int64_t i = scopes.size() - 1; i >= 0; --i) {
        auto *scope = &scopes.at(i);
        if (scope->has_var(name)) {
            *owner_scope = scope;
            return scope->get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        *owner_scope = &main_scope;
        return main_scope.get_var(name);
    }
    return nullptr;
}

P4Z3Instance *P4State::find_var(cstring name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_var(name)) {
            return scope.get_var(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_var(name)) {
        return main_scope.get_var(name);
    }
    return nullptr;
}

void P4State::update_var(cstring name, P4Z3Instance *var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope != nullptr) {
        target_scope->update_var(name, var);
    } else {
        FATAL_ERROR("Variable %s not found.", name);
    }
}

void P4State::declare_var(cstring name, P4Z3Instance *var, const IR::Type *decl_type) {
    if (scopes.empty()) {
        // Assume we insert into the global scope.
        main_scope.declare_var(name, var, decl_type);
    } else {
        get_mut_current_scope()->declare_var(name, var, decl_type);
    }
}

const P4Declaration *P4State::get_static_decl(cstring name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_static_decl(name)) {
            return scope.get_static_decl(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_static_decl(name)) {
        return main_scope.get_static_decl(name);
    }
    error("Static Declaration %s not found in scope.", name);
    exit(1);
}

const IR::Node *P4State::resolve_callable(cstring name,
                                          const IR::Vector<IR::Argument> &arguments) const {
    const P4Scope *owner = &main_scope;
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_static_decl(name)) {
            owner = &scope;
            break;
        }
    }
    const IR::Node *result = nullptr;
    for (const auto *declaration : owner->get_overloads(name)) {
        const auto *node = declaration->get_decl();
        const IR::ParameterList *parameters = nullptr;
        if (const auto *function = node->to<IR::Function>())
            parameters = function->getParameters();
        else if (const auto *method = node->to<IR::Method>())
            parameters = method->getParameters();
        else if (const auto *action = node->to<IR::P4Action>())
            parameters = action->getParameters();
        if (!parameters || !arguments_match(*parameters, arguments, node->is<IR::P4Action>()))
            continue;
        BUG_CHECK(!result, "Ambiguous callable %s", name);
        result = node;
    }
    BUG_CHECK(result, "No matching overload for %s", name);
    return result;
}

P4Declaration *P4State::find_static_decl(cstring name) const {
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_static_decl(name)) {
            return scope.get_static_decl(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_static_decl(name)) {
        return main_scope.get_static_decl(name);
    }
    return nullptr;
}

P4Declaration *P4State::find_static_decl(cstring name, P4Scope **owner_scope) {
    for (int64_t i = scopes.size() - 1; i >= 0; --i) {
        auto *scope = &scopes.at(i);
        if (scope->has_static_decl(name)) {
            *owner_scope = scope;
            return scope->get_static_decl(name);
        }
    }
    // also check the parent scope
    if (main_scope.has_static_decl(name)) {
        *owner_scope = &main_scope;
        return main_scope.get_static_decl(name);
    }
    return nullptr;
}

void P4State::declare_static_decl(cstring name, P4Declaration *decl) {
    // TODO: There is some weird pointer comparison going on.
    // P4Scope *target_scope = nullptr;
    // find_static_decl(name, &target_scope);
    // if (target_scope != nullptr) {
    //     warning("Declaration %s shadows existing declaration.", decl->decl);
    // }
    if (scopes.empty()) {
        main_scope.declare_static_decl(name, decl);
        // assume we insert into the global scope
    } else {
        get_mut_current_scope()->declare_static_decl(name, decl);
    }
}

ProgState P4State::clone_state() const {
    auto cloned_state = ProgState();

    for (const auto &scope : scopes) {
        cloned_state.push_back(scope.clone());
    }
    return cloned_state;
}

VarMap P4State::clone_vars() const {
    VarMap cloned_vars;
    // this also implicitly shadows
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        auto sub_vars = scope.clone_vars();
        cloned_vars.insert(sub_vars.begin(), sub_vars.end());
    }
    return cloned_vars;
}

VarMap P4State::clone_vars(const std::set<cstring> &names) const {
    VarMap result;
    for (const auto &entry : get_vars()) {
        if (names.count(entry.first)) {
            result.emplace(entry.first,
                           std::make_pair(entry.second.first->copy(), entry.second.second));
        }
    }
    return result;
}

VarMap P4State::get_vars() const {
    VarMap concat_map;
    // this also implicitly shadows
    for (const auto &scope : boost::adaptors::reverse(scopes)) {
        auto sub_vars = scope.get_var_map();
        concat_map.insert(sub_vars.begin(), sub_vars.end());
    }
    return concat_map;
}

void P4State::restore_vars(const VarMap &input_map) {
    for (const auto &map_tuple : input_map) {
        update_var(map_tuple.first, map_tuple.second.first);
    }
}

void P4State::merge_vars(const z3::expr &cond, const VarMap &then_map) const {
    for (const auto &map_tuple : get_vars()) {
        const auto else_name = map_tuple.first;
        auto *instance = map_tuple.second.first;
        // TODO: This check should not be necessary
        // Find a cleaner way using scopes
        auto then_instance = then_map.find(else_name);
        if (then_instance != then_map.end()) {
            instance->merge(cond, *then_instance->second.first);
        }
    }
}

void merge_var_maps(const z3::expr &cond, const VarMap &then_map, const VarMap &else_map) {
    for (const auto &then_tuple : then_map) {
        const auto then_name = then_tuple.first;
        auto *then_var = then_tuple.second.first;
        // TODO: This check should not be necessary
        // Find a cleaner way using scopes
        auto else_var = else_map.find(then_name);
        if (else_var != else_map.end()) {
            then_var->merge(cond, *else_var->second.first);
        }
    }
}

void P4State::merge_state(const z3::expr &cond, const ProgState &else_state) {
    for (size_t i = 0; i < scopes.size(); ++i) {
        auto *then_scope = &scopes[i];
        const auto *else_scope = &else_state.at(i);
        merge_var_maps(cond, then_scope->get_var_map(), else_scope->get_var_map());
    }
}
}  // namespace P4::ToZ3
