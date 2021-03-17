#include "type_complex.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

StructBase::StructBase(P4State *state, const IR::Type_StructLike *type,
                       uint64_t member_id)
    : state(state), member_id(member_id), p4_type(type) {
    width = 0;
    auto flat_id = member_id;

    for (auto field : type->fields) {
        cstring name = cstring(std::to_string(flat_id));
        const IR::Type *resolved_type = state->resolve_type(field->type);
        auto member_var = state->gen_instance(name, resolved_type, flat_id);
        if (auto si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_member_map()->size();
        } else if (auto tbi = resolved_type->to<IR::Type_Bits>()) {
            width += tbi->width_bits();
            flat_id++;
        } else if (auto tvb = resolved_type->to<IR::Type_Varbits>()) {
            width += tvb->width_bits();
            flat_id++;
        } else if (resolved_type->is<IR::Type_Boolean>()) {
            width++;
            flat_id++;
        } else {
            BUG("Type \"%s\" not supported!.", field->type);
        }
        insert_member(field->name.name, member_var);
        member_types.insert({field->name.name, resolved_type});
    }
}

StructBase::StructBase(const StructBase &other) : P4Z3Instance(other) {
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
}

StructBase &StructBase::operator=(const StructBase &other) {
    if (this == &other) {
        return *this;
    }
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto &value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
    return *this;
}

void StructBase::set_undefined() {
    for (auto member_tuple : members) {
        member_tuple.second->set_undefined();
    }
}

void StructBase::set_list(std::vector<P4Z3Instance *> input_list) {
    size_t idx = 0;
    for (auto member_tuple = members.begin(); member_tuple != members.end();
         ++member_tuple) {
        auto member_name = member_tuple->first;
        auto target_val = member_tuple->second;
        auto input_val = input_list.at(idx);
        if (auto sub_list = input_val->to<ListInstance>()) {
            if (auto sub_target = target_val->to_mut<StructBase>()) {
                sub_target->set_list(sub_list->get_val_list());
            } else {
                BUG("Unsupported set list class.");
            }
        } else {
            auto member_type = get_member_type(member_name);
            auto cast_val = input_val->cast_allocate(member_type);
            update_member(member_name, cast_val);
        }
        idx++;
    }
}

void StructBase::merge(const z3::expr &cond, const P4Z3Instance &other) {
    auto other_struct = other.to<StructBase>();

    if (!other_struct) {
        BUG("Unsupported merge class.");
    }

    for (auto member_tuple : members) {
        cstring member_name = member_tuple.first;
        auto then_var = member_tuple.second;
        auto else_var = other_struct->get_const_member(member_name);
        then_var->merge(cond, *else_var);
    }
}

P4Z3Instance *StructBase::cast_allocate(const IR::Type *dest_type) const {
    // There is only rudimentary casting support for Type_Structs
    if (auto tn = dest_type->to<IR::Type_Name>()) {
        dest_type = state->resolve_type(tn);
    }
    if (dest_type == p4_type) {
        return copy();
    }
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s",
                      p4_type->node_type_name(), dest_type->node_type_name());
}

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : StructBase(state, type, member_id),
      valid(state->get_z3_ctx()->bool_val(true)) {}

StructInstance *StructInstance::copy() const {
    return new StructInstance(*this);
}

void StructInstance::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr) {
        valid = *valid_expr;
    }
    for (auto member_tuple : members) {
        auto member = member_tuple.second;
        if (auto z3_var = member->to_mut<StructInstance>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

void StructInstance::set_valid(const z3::expr &valid_val) { valid = valid_val; }
const z3::expr *StructInstance::get_valid() const { return &valid; }

std::vector<std::pair<cstring, z3::expr>>
StructInstance::get_z3_vars(cstring prefix, const z3::expr *valid_expr) const {
    const z3::expr *tmp_valid;
    if (this->is<HeaderInstance>() && valid_expr == nullptr) {
        valid_expr = &valid;
        tmp_valid = valid_expr;
    } else if (valid_expr) {
        tmp_valid = valid_expr;
    } else {
        tmp_valid = &valid;
    }
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        auto member = member_tuple.second;
        if (auto *z3_var = member->to<Z3Bitvector>()) {
            auto dest_type = member_types.at(member_tuple.first);
            auto invalid_var = state->gen_z3_expr("invalid", dest_type);
            auto valid_var = z3::ite(*tmp_valid, z3_var->val, invalid_var);
            z3_vars.push_back({name, valid_var});
        } else if (auto z3_var = member->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (auto z3_var = member->to<Z3Int>()) {
            // We receive an int that we need to cast towards the member
            // type
            auto dest_type = member_types.at(member_tuple.first);
            auto cast_val =
                z3::int2bv(dest_type->width_bits(), z3_var->val).simplify();
            auto invalid_var = state->gen_z3_expr("invalid", dest_type);
            auto valid_var = z3::ite(*tmp_valid, cast_val, invalid_var);
            z3_vars.push_back({name, valid_var});
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }
    return z3_vars;
}

StructInstance::StructInstance(const StructInstance &other)
    : StructBase(other), valid(other.valid) {}

StructInstance &StructInstance::operator=(const StructInstance &other) {
    if (this == &other) {
        return *this;
    }
    valid = other.valid;
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
    return *this;
}

HeaderInstance::HeaderInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : StructInstance(state, type, member_id) {
    valid = state->get_z3_ctx()->bool_val(false);
    member_functions["setValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setValid(visitor); });
    member_functions["setInvalid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setInvalid(visitor); });
    member_functions["isValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { isValid(visitor); });
}

HeaderInstance::HeaderInstance(const HeaderInstance &other)
    : StructInstance(other) {
    member_functions["setValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setValid(visitor); });
    member_functions["setInvalid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setInvalid(visitor); });
    member_functions["isValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { isValid(visitor); });
}

HeaderInstance &HeaderInstance::operator=(const HeaderInstance &other) {
    // TODO: Clean this up and call super functions
    if (this == &other) {
        return *this;
    }
    valid = other.valid;
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
    member_functions["setValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setValid(visitor); });
    member_functions["setInvalid0"] =
        new FunctionWrapper([this](Visitor *visitor) { setInvalid(visitor); });
    member_functions["isValid0"] =
        new FunctionWrapper([this](Visitor *visitor) { isValid(visitor); });
    return *this;
}

void HeaderInstance::setValid(Visitor *) {
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::setInvalid(Visitor *) {
    valid = state->get_z3_ctx()->bool_val(false);
    propagate_validity(&valid);
    set_undefined();
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::isValid(Visitor *) {
    static Z3Bitvector wrapper = Z3Bitvector(state, valid);
    state->set_expr_result(wrapper);
}

void HeaderInstance::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr) {
        valid = *valid_expr;
    } else {
        cstring name = std::to_string(member_id) + "_valid";
        valid = state->get_z3_ctx()->bool_const(name);
        valid_expr = &valid;
    }
    for (auto member_tuple : members) {
        auto member = member_tuple.second;
        if (auto z3_var = member->to_mut<StructInstance>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

HeaderInstance *HeaderInstance::copy() const {
    return new HeaderInstance(*this);
}

void HeaderInstance::merge(const z3::expr &cond, const P4Z3Instance &other) {
    auto other_struct = other.to<HeaderInstance>();

    if (!other_struct) {
        BUG("Unsupported merge class.");
    }
    StructBase::merge(cond, other);
    auto valid_merge = z3::ite(cond, *get_valid(), *other_struct->get_valid());
    set_valid(valid_merge);
}
void HeaderInstance::set_list(std::vector<P4Z3Instance *> input_list) {
    StructBase::set_list(input_list);
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
}

EnumInstance::EnumInstance(P4State *p4_state, const IR::Type_Enum *type,
                           uint64_t ext_member_id)
    : p4_type(type) {
    // FIXME: Enums should not be a struct base, actually
    width = 32;
    state = p4_state;
    member_id = ext_member_id;
    const auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        insert_member(name, member_var);
    }
}

std::vector<std::pair<cstring, z3::expr>>
EnumInstance::get_z3_vars(cstring prefix, const z3::expr *) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    auto z3_const = state->get_z3_ctx()->constant(
        p4_type->name.name, state->get_z3_ctx()->bv_sort(32));
    cstring name = std::to_string(member_id);
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    z3_vars.push_back({name, z3_const});
    return z3_vars;
}

ErrorInstance::ErrorInstance(P4State *p4_state, const IR::Type_Error *type,
                             uint64_t ext_member_id)
    : p4_type(type) {
    // FIXME: Enums should not be a struct base, actually
    width = 32;
    state = p4_state;
    member_id = ext_member_id;
    auto member_type = new IR::Type_Bits(32, false);
    for (auto member : type->members) {
        cstring name = member->name.name;
        auto member_var = state->gen_instance(name, member_type);
        insert_member(p4_type->name.name, member_var);
    }
}

std::vector<std::pair<cstring, z3::expr>>
ErrorInstance::get_z3_vars(cstring prefix, const z3::expr *) const {
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    cstring name = p4_type->name.name;
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    auto z3_const =
        state->get_z3_ctx()->constant(name, state->get_z3_ctx()->bv_sort(32));
    z3_vars.push_back({std::to_string(member_id), z3_const});
    return z3_vars;
}

ErrorInstance *ErrorInstance::copy() const {
    return new ErrorInstance(state, p4_type, member_id);
}

ExternInstance::ExternInstance(const IR::Type_Extern *type) : p4_type(type) {
    for (auto method : type->methods) {
        auto method_identifier =
            method->getName().name +
            std::to_string(method->getParameters()->size());
        methods.insert({method_identifier, new P4Declaration(method)});
    }
}

P4Z3Instance *ListInstance::cast_allocate(const IR::Type *dest_type) const {
    auto instance = state->gen_instance("list", dest_type);
    auto struct_instance = instance->to_mut<StructBase>();
    if (struct_instance == nullptr) {
        BUG("Unsupported type %s for ListInstance.",
            dest_type->node_type_name());
    }
    struct_instance->set_list(val_list);
    return struct_instance;
}

ListInstance *ListInstance::copy() const {
    // std::vector<P4Z3Instance *> val_list_copy;
    // for (auto val : val_list) {
    //     val_list_copy.push_back(val->copy());
    // }
    // we perform a copy any time we assign a list instance
    // so cloning is not needed here
    return new ListInstance(state, val_list, p4_type);
}

P4TableInstance::P4TableInstance(P4State *state, const IR::Declaration *decl)
    : P4Declaration(decl), state(state),
      hit(state->get_z3_ctx()->bool_val(false)) {
    members.insert({"action_run", this});
    cstring apply_str = "apply";
    if (auto table = decl->to<IR::P4Table>()) {
        apply_str += std::to_string(table->getApplyParameters()->size());
    }
    member_functions[apply_str] =
        new FunctionWrapper([this](Visitor *visitor) { apply(visitor); });
}
P4TableInstance::P4TableInstance(
    P4State *state, const IR::Declaration *decl, cstring table_name,
    const z3::expr hit, std::vector<const IR::KeyElement *> keys,
    std::vector<const IR::MethodCallExpression *> actions, bool immutable)
    : P4Declaration(decl), state(state), table_name(table_name), hit(hit),
      keys(keys), actions(actions), immutable(immutable) {
    members.insert({"action_run", this});
    members.insert({"hit", new Z3Bitvector(state, hit)});
    cstring apply_str = "apply";
    if (auto table = decl->to<IR::P4Table>()) {
        apply_str += std::to_string(table->getApplyParameters()->size());
    }
    member_functions[apply_str] =
        new FunctionWrapper([this](Visitor *visitor) { apply(visitor); });
}

void P4TableInstance::apply(Visitor *) { state->set_expr_result(this); }

DeclarationInstance::DeclarationInstance(P4State *state,
                                         const IR::Type_Declaration *decl)
    : state(state), decl(decl) {
    cstring apply_str = "apply";
    if (auto ctrl = decl->to<IR::P4Control>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    } else if (auto ctrl = decl->to<IR::P4Parser>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    }
    member_functions[apply_str] =
        new FunctionWrapper([this](Visitor *visitor) { apply(visitor); });
}

void DeclarationInstance::apply(Visitor *) { state->set_expr_result(this); }

} // namespace TOZ3_V2
