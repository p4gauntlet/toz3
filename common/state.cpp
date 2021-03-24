#include "state.h"

#include <complex>
#include <cstdio>
#include <ostream>

#include "lib/exceptions.h"

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

z3::expr compute_slice(const z3::expr &lval, const z3::expr &rval,
                       const z3::expr &hi, const z3::expr &lo) {
    auto ctx = &lval.get_sort().ctx();
    auto lval_max = lval.get_sort().bv_size() - 1ull;
    auto lval_min = 0ull;
    auto hi_int = hi.get_numeral_uint64();
    auto lo_int = lo.get_numeral_uint64();
    if (hi_int == lval_max && lo_int == lval_min) {
        return rval;
    }
    z3::expr_vector assemble(*ctx);
    if (hi_int < lval_max) {
        assemble.push_back(lval.extract(lval_max, hi_int + 1));
    }
    // auto middle_size = ctx->bv_sort(hi_int + 1 - lo_int);
    // auto cast_val = pure_bv_cast(rval, middle_size);
    assemble.push_back(rval);

    if (lo_int > lval_min) {
        assemble.push_back(lval.extract(lo_int - 1, lval_min));
    }

    return z3::concat(assemble);
}

Z3Bitvector *produce_slice(P4State *state, Visitor *visitor,
                           const IR::Slice *sl, const P4Z3Instance *val) {
    // FIXME: What to do about repeated evaluation?
    const z3::expr *lval = nullptr;
    const z3::expr *rval = nullptr;
    const z3::expr *hi = nullptr;
    const z3::expr *lo = nullptr;
    bool is_signed = false;
    // FIXME: A little snag in the way we return values...
    val = val->copy();
    if (auto z3_bitvec = val->to<Z3Bitvector>()) {
        rval = &z3_bitvec->val;
        is_signed = z3_bitvec->is_signed;
    } else if (auto z3_int = val->to<Z3Int>()) {
        rval = &z3_int->val;
    } else {
        P4C_UNIMPLEMENTED("Unsupported rval of type %s for slice.",
                          val->get_static_type());
    }
    visitor->visit(sl->e0);
    auto lval_expr = state->copy_expr_result();
    if (auto z3_bitvec = lval_expr->to<Z3Bitvector>()) {
        lval = &z3_bitvec->val;
    } else {
        P4C_UNIMPLEMENTED("Unsupported lval of type %s for slice.",
                          val->get_static_type());
    }
    visitor->visit(sl->e1);
    auto hi_expr = state->copy_expr_result();
    if (auto z3_bitvec = hi_expr->to<Z3Bitvector>()) {
        hi = &z3_bitvec->val;
    } else if (auto z3_int = hi_expr->to<Z3Int>()) {
        hi = &z3_int->val;
    } else {
        P4C_UNIMPLEMENTED("Unsupported hi of type %s for slice.",
                          val->get_static_type());
    }
    visitor->visit(sl->e2);
    auto lo_expr = state->get_expr_result();
    if (auto z3_bitvec = lo_expr->to<Z3Bitvector>()) {
        lo = &z3_bitvec->val;
    } else if (auto z3_int = lo_expr->to<Z3Int>()) {
        lo = &z3_int->val;
    } else {
        P4C_UNIMPLEMENTED("Unsupported lo of type %s for slice.",
                          val->get_static_type());
    }
    auto slice_expr = compute_slice(*lval, *rval, *hi, *lo).simplify();
    return new Z3Bitvector(state, slice_expr, is_signed);
}

StructBase *resolve_reference(P4State *state, Visitor *visitor,
                              const IR::Expression *expr) {
    // We actually get a reference here, not a copy!
    P4Z3Instance *complex_class;
    if (auto member = expr->to<IR::Member>()) {
        auto parent = resolve_reference(state, visitor, member->expr);
        complex_class = parent->get_member(member->member.name);
    } else if (auto name = expr->to<IR::PathExpression>()) {
        complex_class = state->get_var(name->path->name);
    } else if (auto a = expr->to<IR::ArrayIndex>()) {
        visitor->visit(a->right);
        auto index = state->copy_expr_result();
        auto parent = resolve_reference(state, visitor, a->left);
        complex_class = parent->to_mut<StackInstance>()->get_member(index);
    } else {
        P4C_UNIMPLEMENTED("Parent Type %s not implemented!",
                          expr->node_type_name());
    }

    return complex_class->to_mut<StructBase>();
}

using StringOrExpr = boost::variant<cstring, z3::expr>;

void P4State::set_var(Visitor *visitor, const IR::Expression *target,
                      P4Z3Instance *rval) {
    if (auto name = target->to<IR::PathExpression>()) {
        auto dest_type = get_var_type(name->path->name.name);
        auto cast_val = rval->cast_allocate(dest_type);
        update_var(name->path->name, cast_val);
        return;
    }
    if (auto sl = target->to<IR::Slice>()) {
        set_var(visitor, sl->e0, produce_slice(this, visitor, sl, rval));
        return;
    }
    auto tmp_target = target;
    std::vector<StringOrExpr> resolve_list;
    StringOrExpr var_to_set;
    cstring main_member;
    bool has_stack = false;

    if (auto member = tmp_target->to<IR::Member>()) {
        tmp_target = member->expr;
        var_to_set = member->member.name;
    } else if (auto a = tmp_target->to<IR::ArrayIndex>()) {
        tmp_target = a->left;
        visitor->visit(a->right);
        auto index = get_expr_result();
        if (auto z3_expr = index->to<Z3Bitvector>()) {
            var_to_set = z3_expr->val.simplify();
        } else if (auto z3_int = index->to<Z3Int>()) {
            var_to_set = z3_int->val.simplify();
        } else {
            P4C_UNIMPLEMENTED("Setting with an index of type %s not "
                              "implemented for stacks.",
                              index->get_static_type());
        }
        has_stack = true;
    }
    while (true) {
        if (auto member = tmp_target->to<IR::Member>()) {
            tmp_target = member->expr;
            resolve_list.push_back(member->member.name);
        } else if (auto a = tmp_target->to<IR::ArrayIndex>()) {
            tmp_target = a->left;
            visitor->visit(a->right);
            auto index = get_expr_result();
            if (auto z3_expr = index->to<Z3Bitvector>()) {
                resolve_list.push_back(z3_expr->val.simplify());
            } else if (auto z3_int = index->to<Z3Int>()) {
                resolve_list.push_back(z3_int->val.simplify());
            } else {
                P4C_UNIMPLEMENTED("Setting with an index of type %s not "
                                  "implemented for stacks.",
                                  index->get_static_type());
            }
            has_stack = true;
        } else if (auto path = tmp_target->to<IR::PathExpression>()) {
            main_member = path->path->name.name;
            break;
        } else {
            P4C_UNIMPLEMENTED("Unknown target %s!", target->node_type_name());
        }
    }
    // Collection phase done
    // Now begins the setting phase...
    if (has_stack) {
        std::vector<P4Z3Instance *> parent_classes;
        std::vector<std::pair<int, z3::expr>> permutation_pairs;
        auto tmp_parent_classes = parent_classes;
        parent_classes.push_back(get_var(main_member));

        // Collect all the headers that need to be set
        for (auto it = resolve_list.rbegin(); it != resolve_list.rend(); ++it) {
            if (auto name = boost::get<cstring>(&*it)) {
                for (auto &parent_class : parent_classes) {
                    tmp_parent_classes.push_back(
                        parent_class->get_member(*name));
                }
                parent_classes = tmp_parent_classes;
                tmp_parent_classes.clear();
            } else if (auto expr = boost::get<z3::expr>(&*it)) {
                for (auto &parent_class : parent_classes) {
                    std::string val_str;
                    if (expr->is_numeral(val_str, 0)) {
                        tmp_parent_classes.push_back(
                            parent_class->get_member(val_str));
                    } else {
                        auto stack_class = parent_class->to_mut<StructBase>();
                        BUG_CHECK(stack_class, "Expected Stack, got %s",
                                  stack_class->get_static_type());
                        auto type_stack =
                            stack_class->p4_type->to<IR::Type_Stack>();
                        CHECK_NULL(type_stack);

                        auto size = type_stack->getSize();
                        for (size_t idx = 0; idx < size; ++idx) {
                            tmp_parent_classes.push_back(
                                parent_class->get_member(std::to_string(idx)));
                            permutation_pairs.push_back({size, *expr});
                        }
                    }
                }
                parent_classes = tmp_parent_classes;
                tmp_parent_classes.clear();
            } else {
                P4C_UNIMPLEMENTED("Member type not implemented.");
            }
        }
        // Set the variable
        if (auto name = boost::get<cstring>(&var_to_set)) {
            for (auto &parent_class : parent_classes) {
                auto complex_class = parent_class->to_mut<StructBase>();
                CHECK_NULL(complex_class);
                auto dest_type = complex_class->get_member_type(*name);
                auto cast_val = rval->cast_allocate(dest_type);
                complex_class->update_member(*name, cast_val);
            }
        } else if (auto expr = boost::get<z3::expr>(&var_to_set)) {
            std::string val_str;
            for (auto &parent_class : parent_classes) {
                auto complex_class = parent_class->to_mut<StructBase>();
                CHECK_NULL(complex_class);
                if (expr->is_numeral(val_str, 0)) {
                    auto dest_type = complex_class->get_member_type(val_str);
                    auto cast_val = rval->cast_allocate(dest_type);
                    complex_class->update_member(val_str, cast_val);
                } else {
                    P4C_UNIMPLEMENTED("Z3 expression index  %s of type %s not "
                                      "implemented for stacks.",
                                      expr->to_string().c_str(),
                                      expr->get_sort().to_string());
                }
            }
        } else {
            P4C_UNIMPLEMENTED("Member type not implemented.");
        }
        return;
    }

    // This is the default mode where we only have strings for a member.
    auto parent_class = get_var(main_member);
    for (auto it = resolve_list.rbegin(); it != resolve_list.rend(); ++it) {
        auto name = boost::get<cstring>(*it);
        parent_class = parent_class->get_member(name);
    }
    auto name = boost::get<cstring>(var_to_set);
    auto complex_class = parent_class->to_mut<StructBase>();
    CHECK_NULL(complex_class);
    auto dest_type = complex_class->get_member_type(name);
    auto cast_val = rval->cast_allocate(dest_type);
    complex_class->update_member(name, cast_val);
}

VarMap P4State::merge_args_with_params(Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args,
                                       const IR::ParameterList *params) {
    VarMap merged_vec;
    size_t arg_len = args->size();
    size_t idx = 0;
    // TODO: Clean this up...
    for (auto param : params->parameters) {
        if (param->direction == IR::Direction::Out) {
            auto instance = gen_instance("undefined", param->type);
            merged_vec.insert({param->name.name, {instance, param->type}});
            idx++;
            continue;
        }
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            visitor->visit(arg->expression);
            // TODO: Cast here
            merged_vec.insert(
                {param->name.name, {copy_expr_result(), param->type}});
        } else {
            auto arg_expr = gen_instance(param->name.name, param->type);
            if (auto complex_arg = arg_expr->to_mut<StructInstance>()) {
                complex_arg->propagate_validity();
            }
            merged_vec.insert({param->name.name, {arg_expr, param->type}});
        }
        idx++;
    }

    return merged_vec;
}

std::vector<std::pair<const IR::Expression *, cstring>>
resolve_args(const IR::Vector<IR::Argument> *args,
             const IR::ParameterList *params) {
    std::vector<std::pair<const IR::Expression *, cstring>> resolved_args;

    size_t arg_len = args->size();
    size_t idx = 0;
    for (auto param : params->parameters) {
        auto direction = param->direction;
        if (direction == IR::Direction::In ||
            direction == IR::Direction::None) {
            idx++;
            continue;
        }
        if (idx < arg_len) {
            const IR::Argument *arg = args->at(idx);
            if (arg->to<IR::Member>()) {
                // TODO: Index
                resolved_args.push_back({arg->expression, param->name.name});
            } else {
                resolved_args.push_back({arg->expression, param->name.name});
            }
        }
        idx++;
    }
    return resolved_args;
}

void P4State::copy_in(Visitor *visitor, const IR::ParameterList *params,
                      const IR::Vector<IR::Argument> *arguments) {
    // at this point, we assume we are dealing with a Declaration
    std::vector<std::pair<const IR::Expression *, cstring>> copy_out_args =
        resolve_args(arguments, params);
    auto merged_args = merge_args_with_params(visitor, arguments, params);

    push_scope();
    for (auto arg_tuple : merged_args) {
        cstring param_name = arg_tuple.first;
        auto arg_val = arg_tuple.second;
        declare_var(param_name, arg_val.first, arg_val.second);
    }
    set_copy_out_args(copy_out_args);
}

void P4State::copy_out(Visitor *visitor) {
    auto copy_out_args = get_copy_out_args();
    // merge all the state of the different return points
    auto return_states = get_return_states();
    for (auto it = return_states.rbegin(); it != return_states.rend(); ++it) {
        merge_vars(it->first, it->second);
    }

    std::vector<P4Z3Instance *> copy_out_vals;
    for (auto arg_tuple : copy_out_args) {
        auto source = arg_tuple.second;
        auto val = get_var(source);
        copy_out_vals.push_back(val);
    }

    pop_scope();
    size_t idx = 0;
    for (auto arg_tuple : copy_out_args) {
        auto target = arg_tuple.first;
        set_var(visitor, target, copy_out_vals[idx]);
        idx++;
    }
}

z3::expr P4State::gen_z3_expr(cstring name, const IR::Type *type) {
    if (auto tbi = type->to<IR::Type_Bits>()) {
        return ctx->bv_const(name, tbi->width_bits());
    } else if (auto tvb = type->to<IR::Type_Varbits>()) {
        return ctx->bv_const(name, tvb->width_bits());
    } else if (type->is<IR::Type_Boolean>()) {
        return ctx->bool_const(name);
    }
    BUG("Type \"%s\" not supported for Z3 expressions!.", type);
}

P4Z3Instance *P4State::gen_instance(cstring name, const IR::Type *type,
                                    uint64_t id) {
    P4Z3Instance *instance;
    if (auto tn = type->to<IR::Type_Name>()) {
        type = resolve_type(tn);
    }
    // FIXME: Split this up to not muddle things.
    if (auto t = type->to<IR::Type_Struct>()) {
        instance = new StructInstance(this, t, id);
    } else if (auto t = type->to<IR::Type_Header>()) {
        instance = new HeaderInstance(this, t, id);
    } else if (auto t = type->to<IR::Type_Enum>()) {
        instance = new EnumInstance(this, t, id);
    } else if (auto t = type->to<IR::Type_Error>()) {
        instance = new ErrorInstance(this, t, id);
    } else if (auto t = type->to<IR::Type_Stack>()) {
        instance = new StackInstance(this, t, id);
    } else if (auto t = type->to<IR::Type_Extern>()) {
        instance = new ExternInstance(this, t);
    } else if (auto t = type->to<IR::P4Control>()) {
        instance = new DeclarationInstance(this, t);
    } else if (auto t = type->to<IR::P4Parser>()) {
        instance = new DeclarationInstance(this, t);
    } else if (type->is<IR::Type_Void>()) {
        instance = new VoidResult();
    } else if (type->is<IR::Type_Base>()) {
        instance = new Z3Bitvector(this, gen_z3_expr(name, type));
    } else {
        P4C_UNIMPLEMENTED("Instance generation for type \"%s\" not supported!.",
                          type->node_type_name());
    }
    return instance;
}

void P4State::push_scope() { scopes.push_back(P4Scope()); }

void P4State::pop_scope() { scopes.pop_back(); }

void P4State::add_type(cstring type_name, const IR::Type *t) {
    P4Scope *target_scope = nullptr;
    find_type(type_name, &target_scope);
    if (target_scope) {
        FATAL_ERROR("Type %s already exists in target scope.", type_name);
    } else {
        if (scopes.empty()) {
            main_scope.add_type(type_name, t);
            // assume we insert into the global scope
        } else {
            get_mut_current_scope()->add_type(type_name, t);
        }
    }
}

const IR::Type *P4State::get_type(cstring type_name) const {
    for (auto &scope : boost::adaptors::reverse(scopes)) {
        if (scope.has_type(type_name)) {
            return scope.get_type(type_name);
        }
    }
    // also check the parent scope
    return main_scope.get_type(type_name);
}

const IR::Type *P4State::resolve_type(const IR::Type *type) const {
    if (auto tn = type->to<IR::Type_Name>()) {
        cstring type_name = tn->path->name.name;
        return get_type(type_name);
    }
    return type;
}

const IR::Type *P4State::find_type(cstring type_name, P4Scope **owner_scope) {
    for (int i = scopes.size() - 1; i >= 0; --i) {
        auto scope = &scopes.at(i);
        if (scope->has_type(type_name)) {
            *owner_scope = scope;
            return scope->get_type(type_name);
        }
    }
    // also check the parent scope
    if (main_scope.has_type(type_name)) {
        *owner_scope = &main_scope;
        return main_scope.get_type(type_name);
    }
    return nullptr;
}

P4Z3Instance *P4State::get_var(cstring name) const {
    for (auto &scope : boost::adaptors::reverse(scopes)) {
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
    for (auto &scope : boost::adaptors::reverse(scopes)) {
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
    for (int i = scopes.size() - 1; i >= 0; --i) {
        auto scope = &scopes.at(i);
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

void P4State::update_var(cstring name, P4Z3Instance *var) {
    P4Scope *target_scope = nullptr;
    find_var(name, &target_scope);
    if (target_scope) {
        target_scope->update_var(name, var);
    } else {
        FATAL_ERROR("Variable %s not found.", name);
    }
}

void P4State::declare_var(cstring name, P4Z3Instance *var,
                          const IR::Type *decl_type) {
    if (scopes.empty()) {
        // assume we insert into the global scope
        main_scope.declare_var(name, var, decl_type);
    } else {
        get_mut_current_scope()->declare_var(name, var, decl_type);
    }
}

const P4Declaration *P4State::get_static_decl(cstring name) const {
    for (auto &scope : boost::adaptors::reverse(scopes)) {
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

P4Declaration *P4State::find_static_decl(cstring name, P4Scope **owner_scope) {
    for (int i = scopes.size() - 1; i >= 0; --i) {
        auto scope = &scopes.at(i);
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
    P4Scope *target_scope = nullptr;
    find_static_decl(name, &target_scope);
    if (target_scope) {
        warning("Declaration %s shadows existing declaration.", decl->decl);
    }
    if (scopes.empty()) {
        main_scope.declare_static_decl(name, decl);
        // assume we insert into the global scope
    } else {
        get_mut_current_scope()->declare_static_decl(name, decl);
    }
}

ProgState P4State::clone_state() const {
    auto cloned_state = ProgState();

    for (auto &scope : scopes) {
        cloned_state.push_back(scope.clone());
    }
    return cloned_state;
}

VarMap P4State::clone_vars() const {
    VarMap cloned_vars;
    // this also implicitly shadows
    for (auto &scope : boost::adaptors::reverse(scopes)) {
        auto sub_vars = scope.clone_vars();
        cloned_vars.insert(sub_vars.begin(), sub_vars.end());
    }
    return cloned_vars;
}

VarMap P4State::get_vars() const {
    VarMap concat_map;
    // this also implicitly shadows
    for (auto &scope : boost::adaptors::reverse(scopes)) {
        auto sub_vars = scope.get_var_map();
        concat_map.insert(sub_vars.begin(), sub_vars.end());
    }
    return concat_map;
}

void P4State::restore_vars(const VarMap &input_map) {
    for (auto &map_tuple : input_map) {
        update_var(map_tuple.first, map_tuple.second.first);
    }
}

void P4State::merge_vars(const z3::expr &cond, const VarMap &then_map) {
    for (auto &map_tuple : get_vars()) {
        auto else_name = map_tuple.first;
        auto instance = map_tuple.second.first;
        // TODO: This check should not be necessary
        // Find a cleaner way using scopes
        auto then_instance = then_map.find(else_name);
        if (then_instance != then_map.end()) {
            instance->merge(cond, *then_instance->second.first);
        }
    }
}

void merge_var_maps(const z3::expr &cond, const VarMap &then_map,
                    const VarMap &else_map) {
    for (auto &then_tuple : then_map) {
        auto then_name = then_tuple.first;
        auto then_var = then_tuple.second.first;
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
        auto then_scope = &scopes[i];
        auto else_scope = &else_state.at(i);
        merge_var_maps(cond, then_scope->get_var_map(),
                       else_scope->get_var_map());
    }
}
} // namespace TOZ3_V2
