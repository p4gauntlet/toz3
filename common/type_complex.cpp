#include "type_complex.h"

#include <cstddef>
#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {
/***
===============================================================================
StructBase
===============================================================================
***/
StructBase::StructBase(P4State *state, const IR::Type *type, uint64_t member_id,
                       cstring prefix)
    : P4Z3Instance(type), state(state),
      valid(state->get_z3_ctx()->bool_val(true)) {
    width = 0;
    instance_name = prefix + std::to_string(member_id);
}

StructBase::StructBase(const StructBase &other)
    : P4Z3Instance(other), valid(other.valid) {
    width = other.width;
    state = other.state;
    instance_name = other.instance_name;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto *member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
}

void StructBase::set_undefined() {
    for (auto member_tuple : members) {
        member_tuple.second->set_undefined();
    }
}

void StructBase::set_list(std::vector<P4Z3Instance *> input_list) {
    size_t idx = 0;
    for (auto &member_tuple : members) {
        auto member_name = member_tuple.first;
        auto *target_val = member_tuple.second;
        const auto *input_val = input_list.at(idx);
        if (const auto *sub_list = input_val->to<ListInstance>()) {
            if (auto *sub_target = target_val->to_mut<StructBase>()) {
                sub_target->set_list(sub_list->get_val_list());
            } else {
                BUG("Unsupported set list class.");
            }
        } else {
            const auto *member_type = get_member_type(member_name);
            auto *cast_val = input_val->cast_allocate(member_type);
            update_member(member_name, cast_val);
        }
        idx++;
    }
}

void StructBase::merge(const z3::expr &cond, const P4Z3Instance &then_expr) {
    const auto *then_struct = then_expr.to<StructBase>();
    BUG_CHECK(then_struct, "Unsupported merge class.");
    for (auto member_tuple : members) {
        cstring member_name = member_tuple.first;
        auto *then_var = member_tuple.second;
        const auto *else_var = then_struct->get_const_member(member_name);
        then_var->merge(cond, *else_var);
    }
}

P4Z3Instance *StructBase::cast_allocate(const IR::Type *dest_type) const {
    // There is only rudimentary casting support for Type_Structs
    if (const auto *tn = dest_type->to<IR::Type_Name>()) {
        dest_type = state->resolve_type(tn);
    }
    if (dest_type == p4_type) {
        return copy();
    }
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s", p4_type,
                      dest_type);
}

void StructBase::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr != nullptr) {
        valid = *valid_expr;
    }
    for (auto member_tuple : members) {
        auto *member = member_tuple.second;
        if (auto *z3_var = member->to_mut<StructBase>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

void StructBase::bind(uint64_t member_id, cstring prefix) {
    instance_name = prefix + std::to_string(member_id);
    auto flat_id = member_id;
    for (auto type_tuple : members) {
        auto member_name = type_tuple.first;
        auto *member_var = type_tuple.second;
        cstring name = prefix + std::to_string(flat_id);
        if (auto *si = member_var->to_mut<StructBase>()) {
            si->bind(flat_id, prefix);
            flat_id += si->get_width();
        } else if (const auto *z3_var = member_var->to<Z3Bitvector>()) {
            const auto *z3_expr = z3_var->get_val();
            auto member_var =
                state->get_z3_ctx()->constant(name, z3_expr->get_sort());
            update_member(member_name,
                          new Z3Bitvector(state, z3_var->get_p4_type(),
                                          member_var, z3_var->is_signed));
            flat_id += z3_var->get_p4_type()->width_bits();
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.",
                              member_var->get_static_type());
        }
    }
}

z3::expr StructBase::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (other.is<StructBase>()) {
        for (auto member_tuple : members) {
            auto member_name = member_tuple.first;
            const auto *member_val = member_tuple.second;
            const auto *other_val = other.get_member(member_name);
            is_eq = is_eq && (member_val->operator==(*other_val));
        }
        return is_eq;
    }
    P4C_UNIMPLEMENTED("Comparing a struct base to %s is not supported.",
                      other.get_static_type());
}

z3::expr StructBase::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

/***
===============================================================================
StructInstance
===============================================================================
***/

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id, cstring prefix)
    : StructBase(state, type, member_id, prefix) {
    auto flat_id = member_id;
    for (const auto *field : type->fields) {
        const IR::Type *resolved_type = state->resolve_type(field->type);
        auto *member_var =
            state->gen_instance(UNDEF_LABEL, resolved_type, flat_id);
        if (auto *si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_width();
        } else if (const auto *tbi = resolved_type->to<IR::Type_Bits>()) {
            width += tbi->width_bits();
            flat_id += tbi->width_bits();
        } else if (const auto *tvb = resolved_type->to<IR::Type_Varbits>()) {
            width += tvb->size;
            flat_id += tvb->size;
        } else if (resolved_type->is<IR::Type_Boolean>()) {
            width++;
            flat_id++;
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.", field->type);
        }
        insert_member(field->name.name, member_var);
        member_types.insert({field->name.name, resolved_type});
    }
}

StructInstance *StructInstance::copy() const {
    return new StructInstance(*this);
}

std::vector<std::pair<cstring, z3::expr>>
StructInstance::get_z3_vars(cstring prefix, const z3::expr *valid_expr) const {
    // TODO: Clean this up and split it
    const z3::expr *tmp_valid = nullptr;
    if (this->is<HeaderInstance>() && valid_expr == nullptr) {
        valid_expr = &valid;
        tmp_valid = valid_expr;
    } else if (valid_expr != nullptr) {
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
        const auto *member = member_tuple.second;
        if (const auto *z3_var = member->to<Z3Bitvector>()) {
            const auto *dest_type = member_types.at(member_tuple.first);
            auto invalid_var = state->gen_z3_expr(INVALID_LABEL, dest_type);
            auto valid_var =
                z3::ite(*tmp_valid, *z3_var->get_val(), invalid_var);
            z3_vars.emplace_back(name, valid_var);
        } else if (const auto *z3_var = member->to<StructBase>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (const auto *z3_var = member->to<Z3Int>()) {
            // We receive an int that we need to cast towards the member
            // type
            const auto *dest_type = member_types.at(member_tuple.first);
            auto cast_val =
                z3::int2bv(dest_type->width_bits(), *z3_var->get_val())
                    .simplify();
            auto invalid_var = state->gen_z3_expr(INVALID_LABEL, dest_type);
            auto valid_var = z3::ite(*tmp_valid, cast_val, invalid_var);
            z3_vars.emplace_back(name, valid_var);
        } else {
            BUG("Var is neither type z3::expr nor P4Z3Instance!");
        }
    }
    return z3_vars;
}

StructInstance::StructInstance(const StructInstance &other)
    : StructBase(other) {}

StructInstance &StructInstance::operator=(const StructInstance &other) {
    if (this == &other) {
        return *this;
    }
    *this = StructInstance(other);
    return *this;
}

/***
===============================================================================
HeaderInstance
===============================================================================
***/

HeaderInstance::HeaderInstance(P4State *state, const IR::Type_Header *type,
                               uint64_t member_id, cstring prefix)
    : StructInstance(state, type, member_id, prefix) {
    valid = state->get_z3_ctx()->bool_val(false);
    member_functions["setValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setValid(visitor, args);
        };
    member_functions["setInvalid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setInvalid(visitor, args);
        };
    member_functions["isValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        };
}

HeaderInstance::HeaderInstance(const HeaderInstance &other)
    : StructInstance(other) {
    member_functions["setValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setValid(visitor, args);
        };
    member_functions["setInvalid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setInvalid(visitor, args);
        };
    member_functions["isValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        };
}

HeaderInstance &HeaderInstance::operator=(const HeaderInstance &other) {
    if (this == &other) {
        return *this;
    }
    *this = HeaderInstance(other);
    return *this;
}

z3::expr HeaderInstance::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (const auto *other_hdr = other.to<HeaderInstance>()) {
        auto other_is_valid = *other_hdr->get_valid();
        for (auto member_tuple : members) {
            auto member_name = member_tuple.first;
            const auto *member_val = member_tuple.second;
            const auto *other_val = other.get_member(member_name);
            is_eq = is_eq && (member_val->operator==(*other_val));
        }
        auto both_invalid = !(valid || other_is_valid);
        auto both_valid_and_eq = (is_eq && valid && other_is_valid);
        return both_invalid || both_valid_and_eq;
    }
    P4C_UNIMPLEMENTED("Comparing a header to %s is not supported.",
                      other.get_static_type());
}

z3::expr HeaderInstance::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

void HeaderInstance::set_valid(const z3::expr &valid_val) {
    valid = valid_val;
    // If this is header is bound to a union we need to invalidate its peers
    if (parent_union != nullptr) {
        parent_union->update_validity(this, valid_val);
    }
}
const z3::expr *HeaderInstance::get_valid() const { return &valid; }

void HeaderInstance::setValid(Visitor *, const IR::Vector<IR::Argument> *) {
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::setInvalid(Visitor *, const IR::Vector<IR::Argument> *) {
    valid = state->get_z3_ctx()->bool_val(false);
    propagate_validity(&valid);
    set_undefined();
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::isValid(Visitor *, const IR::Vector<IR::Argument> *) {
    state->set_expr_result(Z3Bitvector(state, &BOOL_TYPE, valid));
}

void HeaderInstance::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr != nullptr) {
        valid = *valid_expr;
    } else {
        cstring name = instance_name + "_valid";
        valid = state->get_z3_ctx()->bool_const(name);
        valid_expr = &valid;
    }
    for (auto member_tuple : members) {
        auto *member = member_tuple.second;
        if (auto *z3_var = member->to_mut<StructInstance>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

HeaderInstance *HeaderInstance::copy() const {
    return new HeaderInstance(*this);
}

void HeaderInstance::merge(const z3::expr &cond, const P4Z3Instance &then_var) {
    const auto *then_struct = then_var.to<HeaderInstance>();

    BUG_CHECK(then_struct, "Unsupported merge class.");
    StructBase::merge(cond, then_var);
    auto valid_merge = z3::ite(cond, *then_struct->get_valid(), valid);
    set_valid(valid_merge);
}
void HeaderInstance::set_list(std::vector<P4Z3Instance *> input_list) {
    StructBase::set_list(input_list);
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
}

void HeaderInstance::bind_to_union(HeaderUnionInstance *union_parent) {
    parent_union = union_parent;
}

/***
===============================================================================
StackInstance
===============================================================================
***/

StackInstance::StackInstance(P4State *state, const IR::Type_Stack *type,
                             uint64_t member_id, cstring prefix)
    : StructBase(state, type, member_id, prefix), nextIndex(Z3Int(state, 0)),
      lastIndex(Z3Int(state, 0)), size(Z3Int(state, type->getSize())),
      int_size(type->getSize()), elem_type(type->elementType) {
    auto flat_id = member_id;
    const IR::Type *resolved_type = state->resolve_type(type->elementType);
    for (size_t idx = 0; idx < int_size; ++idx) {
        cstring name = prefix + std::to_string(flat_id);
        auto *member_var = state->gen_instance(name, resolved_type, flat_id);
        if (auto *si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_member_map()->size();
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.",
                              member_var->get_static_type());
        }
        cstring member_name = std::to_string(idx);
        insert_member(member_name, member_var);
        member_types.insert({member_name, resolved_type});
    }
    member_functions["push_front1"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            push_front(visitor, args);
        };
    member_functions["pop_front1"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            pop_front(visitor, args);
        };
}

StackInstance *StackInstance::copy() const { return new StackInstance(*this); }

StackInstance::StackInstance(const StackInstance &other)
    : StructBase(other), nextIndex(other.nextIndex), lastIndex(other.lastIndex),
      size(other.size), int_size(other.int_size), elem_type(other.elem_type) {
    member_functions["push_front1"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            push_front(visitor, args);
        };
    member_functions["pop_front1"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            pop_front(visitor, args);
        };
}

StackInstance &StackInstance::operator=(const StackInstance &other) {
    if (this == &other) {
        return *this;
    }
    *this = StackInstance(other);
    return *this;
}

P4Z3Instance *StackInstance::get_member(cstring name) const {
    if (name == "size") {
        return &this->size;
    }
    if (name == "nextIndex") {
        return &this->nextIndex;
    }
    if (name == "nextIndex") {
        return &this->lastIndex;
    }
    return StructBase::get_member(name);
}

P4Z3Instance *StackInstance::get_member(const z3::expr &index) const {
    auto val = index.simplify();
    std::string val_str;
    if (val.is_numeral(val_str, 0)) {
        return get_member(val_str);
    }
    // We create a new header that we return
    // This header is the merge of all the sub headers of this stack
    auto *base_hdr = state->gen_instance(UNDEF_LABEL, elem_type);
    for (size_t idx = 0; idx < int_size; ++idx) {
        cstring member_name = std::to_string(idx);
        const auto *hdr = get_member(member_name);
        auto z3_int = state->get_z3_ctx()->num_val(idx, val.get_sort());
        base_hdr->merge(val == z3_int, *hdr);
    }
    return base_hdr;
}

void StackInstance::push_front(Visitor *, const IR::Vector<IR::Argument> *) {
    for (size_t i = 0; i < int_size; ++i) {
    }
    P4C_UNIMPLEMENTED("push_front not implemented");
}
void StackInstance::pop_front(Visitor *, const IR::Vector<IR::Argument> *) {
    for (size_t i = 0; i < int_size; ++i) {
    }
    P4C_UNIMPLEMENTED("pop_front not implemented");
}

std::vector<std::pair<cstring, z3::expr>>
StackInstance::get_z3_vars(cstring prefix, const z3::expr *valid_expr) const {
    // TODO: Clean this up and split it
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        const auto *member = member_tuple.second;
        if (const auto *z3_var = member->to<HeaderInstance>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else if (const auto *z3_var = member->to<HeaderUnionInstance>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else {
            BUG("Var is neither type z3::expr nor HeaderInstance!");
        }
    }
    return z3_vars;
}

/***
===============================================================================
HeaderUnionInstance
===============================================================================
***/

HeaderUnionInstance::HeaderUnionInstance(P4State *state,
                                         const IR::Type_HeaderUnion *type,
                                         uint64_t member_id, cstring prefix)
    : StructBase(state, type, member_id, prefix) {
    auto flat_id = member_id;
    for (const auto *field : type->fields) {
        const IR::Type *resolved_type = state->resolve_type(field->type);
        if (resolved_type->is<IR::Type_Header>()) {
            auto *member_var =
                state->gen_instance(UNDEF_LABEL, resolved_type, flat_id);
            const auto *si = member_var->to<HeaderInstance>();
            BUG_CHECK(si, "Unexpected generated instance %s",
                      member_var->to_string());
            width += si->get_width();
            flat_id += si->get_width();
            insert_member(field->name.name, member_var);
            member_types.insert({field->name.name, resolved_type});
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!", field->type);
        }
    }
    member_functions["isValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        };
}

HeaderUnionInstance::HeaderUnionInstance(const HeaderUnionInstance &other)
    : StructBase(other) {
    member_functions["isValid0"] =
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        };
}

HeaderUnionInstance &
HeaderUnionInstance::operator=(const HeaderUnionInstance &other) {
    if (this == &other) {
        return *this;
    }
    *this = HeaderUnionInstance(other);
    return *this;
}

std::vector<std::pair<cstring, z3::expr>>
HeaderUnionInstance::get_z3_vars(cstring prefix,
                                 const z3::expr *valid_expr) const {
    // TODO: Fix this.
    auto tmp_valid = get_valid();
    if (valid_expr == nullptr) {
        valid_expr = &tmp_valid;
    }
    std::vector<z3::expr> valid_vars;

    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    for (auto member_tuple : members) {
        cstring name = member_tuple.first;
        if (prefix.size() != 0) {
            name = prefix + "." + name;
        }
        const auto *member = member_tuple.second;
        if (const auto *hi = member->to<HeaderInstance>()) {
            auto z3_sub_vars = hi->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else {
            BUG("Member is not a header instance!");
        }
    }
    return z3_vars;
}
z3::expr HeaderUnionInstance::get_valid() const {
    z3::expr valid_var = state->get_z3_ctx()->bool_val(false);
    // A header union is valid if any of its members is valid
    for (const auto &member : members) {
        const auto *hi = member.second->to<HeaderInstance>();
        BUG_CHECK(hi, "Unexpected instance %s", member.second->to_string());
        valid_var = valid_var || *hi->get_valid();
    }
    return valid_var;
}

void HeaderUnionInstance::isValid(Visitor *, const IR::Vector<IR::Argument> *) {
    state->set_expr_result(Z3Bitvector(state, &BOOL_TYPE, get_valid()));
}

HeaderUnionInstance *HeaderUnionInstance::copy() const {
    return new HeaderUnionInstance(*this);
}

void HeaderUnionInstance::update_validity(const HeaderInstance *child,
                                          const z3::expr &valid_val) {
    for (auto &member : members) {
        auto *hi = member.second->to_mut<HeaderInstance>();
        BUG_CHECK(hi, "Unexpected instance %s", member.second->to_string());
        const auto *old_valid = hi->get_valid();
        // This is kind of stup but works, I have no means to check child
        // equality yet
        hi->valid = z3::ite(valid_val, state->get_z3_ctx()->bool_val(false),
                            *old_valid);
    }
}

/***
===============================================================================
EnumBase
===============================================================================
***/

EnumBase::EnumBase(P4State *state, const IR::Type *type, uint64_t member_id,
                   cstring prefix)
    : StructBase(state, type, member_id, prefix),
      enum_val(state->gen_z3_expr(instance_name, &P4_STD_BIT_TYPE)) {}

std::vector<std::pair<cstring, z3::expr>>
EnumBase::get_z3_vars(cstring prefix, const z3::expr *valid_expr) const {
    // TODO: Clean this up and split it
    const z3::expr *tmp_valid = nullptr;
    if (valid_expr != nullptr) {
        tmp_valid = valid_expr;
    } else {
        tmp_valid = &valid;
    }
    std::vector<std::pair<cstring, z3::expr>> z3_vars;
    cstring name = instance_name;
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    auto invalid_var = state->gen_z3_expr(INVALID_LABEL, member_type);
    auto valid_var = z3::ite(*tmp_valid, enum_val, invalid_var);
    z3_vars.emplace_back(instance_name, valid_var);
    return z3_vars;
}

void EnumBase::add_enum_member(cstring error_name) {
    auto *member_var = new Z3Bitvector(state, member_type, enum_val);
    insert_member(error_name, member_var);
}

void EnumBase::bind(uint64_t member_id, cstring prefix) {
    instance_name = prefix + std::to_string(member_id);
    auto flat_id = member_id;
    cstring name = prefix + std::to_string(flat_id);
    enum_val = state->gen_z3_expr(name, member_type);
}

z3::expr EnumBase::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (const auto *other_numeric = other.to<NumericVal>()) {
        auto other_val = *other_numeric->get_val();
        auto cast_val = pure_bv_cast(enum_val, other_val.get_sort());
        return cast_val == other_val;
    }
    if (const auto *other_enum = other.to<EnumBase>()) {
        return enum_val == other_enum->get_enum_val();
    }
    P4C_UNIMPLEMENTED("Comparing a enum base to %s is not supported.",
                      other.get_static_type());
}

z3::expr EnumBase::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

z3::expr EnumBase::get_enum_val() const { return enum_val; }
void EnumBase::set_enum_val(const z3::expr &enum_input) {
    enum_val = enum_input;
}

void EnumBase::merge(const z3::expr &cond, const P4Z3Instance &then_expr) {
    const auto *then_enum = then_expr.to<EnumBase>();
    BUG_CHECK(then_enum, "Unsupported merge class.");
    enum_val = z3::ite(cond, then_enum->get_enum_val(), enum_val);
}

/***
===============================================================================
EnumInstance
===============================================================================
***/

EnumInstance::EnumInstance(P4State *p4_state, const IR::Type_Enum *type,
                           uint64_t ext_member_id, cstring prefix)
    : EnumBase(p4_state, type, ext_member_id, prefix) {
    // FIXME: Enums should not be a struct base, actually
    width = 32;
    size_t idx = 0;
    for (const auto *member : type->members) {
        auto *member_var = new Z3Bitvector(
            state, member_type, state->get_z3_ctx()->bv_val(idx, 32));
        insert_member(member->name.name, member_var);
        member_types.insert({member->name.name, member_type});
        idx++;
    }
}

EnumInstance *EnumInstance::copy() const { return new EnumInstance(*this); }

/***
===============================================================================
ErrorInstance
==================================d=============================================
***/

ErrorInstance::ErrorInstance(P4State *p4_state, const IR::Type_Error *type,
                             uint64_t ext_member_id, cstring prefix)
    : EnumBase(p4_state, type, ext_member_id, prefix) {
    // FIXME: Enums should not be a struct base, actually
    width = 32;
    size_t idx = 0;
    for (const auto *member : type->members) {
        auto *member_var = new Z3Bitvector(
            state, member_type, state->get_z3_ctx()->bv_val(idx, 32));
        insert_member(member->name.name, member_var);
        member_types.insert({member->name.name, member_type});
        idx++;
    }
}

ErrorInstance *ErrorInstance::copy() const { return new ErrorInstance(*this); }

/***
===============================================================================
SerEnumInstance
===============================================================================
***/

SerEnumInstance::SerEnumInstance(
    P4State *p4_state, ordered_map<cstring, P4Z3Instance *> input_members,
    const IR::Type_SerEnum *type, uint64_t ext_member_id, cstring prefix)
    : EnumBase(p4_state, type, ext_member_id, prefix) {
    enum_val = state->gen_z3_expr(instance_name, type->type);
    if (const auto *tb = type->type->to<IR::Type_Bits>()) {
        member_type = tb;
        width = tb->width_bits();
    } else {
        P4C_UNIMPLEMENTED("Type %s not supported for SerEnum!",
                          type->type->node_type_name());
    }
    members = std::move(input_members);
}

SerEnumInstance *SerEnumInstance::copy() const {
    return new SerEnumInstance(*this);
}

/***
===============================================================================
ExternInstance
===============================================================================
***/

ExternInstance::ExternInstance(P4State *state, const IR::Type_Extern *p4_type)
    : P4Z3Instance(p4_type), state(state), extern_type(p4_type) {
    for (const auto *method : p4_type->methods) {
        // FIXME: Overloading uses num of parameters, it should use types
        cstring overloaded_name = method->name.name;
        auto num_params = 0;
        auto num_optional_params = 0;
        for (const auto *param : method->getParameters()->parameters) {
            if (param->isOptional()) {
                num_optional_params += 1;
            } else {
                num_params += 1;
            }
        }
        for (auto idx = 0; idx <= num_optional_params; ++idx) {
            // The IR has bizarre side effects when storing pointers in a map
            // FIXME: Think about how to simplify this, maybe use their vector
            cstring name = overloaded_name + std::to_string(num_params + idx);
            methods.insert({name, method});
        }
    }
}

/***
===============================================================================
ListInstance
===============================================================================
***/
ListInstance::ListInstance(P4State *state,
                           const std::vector<P4Z3Instance *> &val_list,
                           const IR::Type *type_list)
    : StructBase(state, type_list, 0, "") {
    IR::Vector<IR::Type> components;
    for (size_t idx = 0; idx < val_list.size(); ++idx) {
        auto *val = val_list[idx];
        cstring name = std::to_string(idx);
        insert_member(name, val);
        const auto *type = val->get_p4_type();
        member_types.insert({name, type});
        components.push_back(type);
    }
    // The list should have the type information now
    p4_type = new IR::Type_List(components);
}

ListInstance::ListInstance(P4State *state, const IR::Type_List *list_type,
                           uint64_t member_id, cstring prefix)
    : StructBase(state, list_type, member_id, prefix) {
    auto flat_id = member_id;
    for (size_t idx = 0; idx < list_type->components.size(); ++idx) {
        const auto *type = list_type->components[idx];
        cstring name = std::to_string(idx);
        insert_member(
            name, state->gen_instance(prefix + std::to_string(flat_id), type));
        member_types.insert({name, type});
        flat_id++;
    }
}

P4Z3Instance *ListInstance::cast_allocate(const IR::Type *dest_type) const {
    auto *instance = state->gen_instance("list", dest_type);
    auto *struct_instance = instance->to_mut<StructBase>();
    if (struct_instance == nullptr) {
        P4C_UNIMPLEMENTED("Unsupported type %s for ListInstance.",
                          dest_type->node_type_name());
    }
    struct_instance->set_list(get_val_list());
    return struct_instance;
}

ListInstance *ListInstance::copy() const {
    // std::vector<P4Z3Instance *> val_list_copy;
    // for (auto val : val_list) {
    //     val_list_copy.push_back(val->copy());
    // }
    // we perform a copy any time we assign a list instance
    // so cloning is not needed here
    return new ListInstance(state, get_val_list(), p4_type);
}

std::vector<P4Z3Instance *> ListInstance::get_val_list() const {
    std::vector<P4Z3Instance *> val_list;
    for (const auto &member : members) {
        val_list.push_back(member.second);
    }
    return val_list;
}

/***
===============================================================================
TupleInstance
===============================================================================
***/

TupleInstance::TupleInstance(P4State *state, const IR::Type_Tuple *type,
                             uint64_t member_id, cstring prefix)
    : StructBase(state, type, member_id, prefix) {
    size_t idx = 0;
    for (const auto &field_type : type->components) {
        const IR::Type *resolved_type = state->resolve_type(field_type);
        auto *member_var =
            state->gen_instance(UNDEF_LABEL, resolved_type, member_id + idx);
        cstring name = std::to_string(idx);
        insert_member(name, member_var);
        member_types.insert({name, resolved_type});
        idx++;
    }
}

TupleInstance *TupleInstance::copy() const { return new TupleInstance(*this); }

/***
===============================================================================
P4TableInstance
===============================================================================
***/

P4TableInstance::P4TableInstance(P4State *state, const IR::Declaration *decl)
    : P4Declaration(decl), state(state),
      hit(state->get_z3_ctx()->bool_val(false)) {
    members.insert({"action_run", this});
    cstring apply_str = "apply";
    if (const auto *table = decl->to<IR::P4Table>()) {
        apply_str += std::to_string(table->getApplyParameters()->size());
    }
    member_functions[apply_str] = [this](Visitor *visitor,
                                         const IR::Vector<IR::Argument> *args) {
        apply(visitor, args);
    };
}
P4TableInstance::P4TableInstance(
    P4State *state, const IR::Declaration *decl, cstring table_name,
    const z3::expr hit, std::vector<const IR::KeyElement *> keys,
    std::vector<const IR::MethodCallExpression *> actions, bool immutable)
    : P4Declaration(decl), state(state), table_name(table_name), hit(hit),
      keys(keys), actions(actions), immutable(immutable) {
    members.insert({"action_run", this});
    members.insert({"hit", new Z3Bitvector(state, &BOOL_TYPE, hit)});
    cstring apply_str = "apply";
    if (const auto *table = decl->to<IR::P4Table>()) {
        apply_str += std::to_string(table->getApplyParameters()->size());
    }
    member_functions[apply_str] = [this](Visitor *visitor,
                                         const IR::Vector<IR::Argument> *args) {
        apply(visitor, args);
    };
}

void P4TableInstance::apply(Visitor *visitor,
                            const IR::Vector<IR::Argument> *args) {
    const auto *table_decl = decl->to<IR::P4Table>();
    CHECK_NULL(table_decl);
    const auto *params = table_decl->getApplyParameters();
    const auto *type_params =
        table_decl->getApplyMethodType()->getTypeParameters();
    const ParamInfo param_info = {*params, *args, *type_params, {}};
    state->copy_in(visitor, param_info);
    visitor->visit(decl);
    state->copy_out();
}

/***
===============================================================================
ControlInstance
===============================================================================
***/

ControlInstance::ControlInstance(
    P4State *state, const IR::Type_Declaration *decl,
    std::vector<P4Z3Instance *> resolved_const_args)
    : P4Z3Instance(nullptr), state(state),
      resolved_const_args(std::move(resolved_const_args)), decl(decl) {
    cstring apply_str = "apply";
    if (const auto *ctrl = decl->to<IR::P4Control>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    } else if (const auto *ctrl = decl->to<IR::P4Parser>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    }
    member_functions[apply_str] = [this](Visitor *visitor,
                                         const IR::Vector<IR::Argument> *args) {
        apply(visitor, args);
    };
}

void ControlInstance::apply(Visitor *visitor,
                            const IR::Vector<IR::Argument> *args) {
    const IR::ParameterList *params = nullptr;
    const IR::ParameterList *const_params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    if (const auto *control = decl->to<IR::P4Control>()) {
        params = control->getApplyParameters();
        const_params = control->getConstructorParameters();
        type_params = control->getApplyMethodType()->getTypeParameters();
    } else if (const auto *parser = decl->to<IR::P4Parser>()) {
        params = parser->getApplyParameters();
        const_params = parser->getConstructorParameters();
        type_params = parser->getApplyMethodType()->getTypeParameters();
    }
    CHECK_NULL(params);
    const ParamInfo param_info = {*params, *args, *type_params, {}};
    state->copy_in(visitor, param_info);
    for (size_t idx = 0; idx < resolved_const_args.size(); ++idx) {
        const auto *const_param = const_params->getParameter(idx);
        state->declare_var(const_param->name.name, resolved_const_args[idx],
                           const_param->type);
    }
    visitor->visit(decl);
    state->copy_out();
}

}  // namespace TOZ3_V2
