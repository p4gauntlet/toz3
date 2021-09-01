#include "type_complex.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include "lib/error.h"
#include "lib/null.h"
#include "state.h"
#include "type_base.h"
#include "type_simple.h"
#include "util.h"
#include "visitor_specialize.h"

namespace TOZ3 {
/***
===============================================================================
StructBase
===============================================================================
***/
StructBase::StructBase(P4State *state, const IR::Type *type, cstring name,
                       uint64_t member_id)
    : P4Z3Instance(type), state(state),
      valid(state->get_z3_ctx()->bool_val(true)), instance_name(name) {
    width = 0;
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
                BUG("Unsupported set list class %s.",
                    target_val->get_static_type());
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
    if (dest_type->equiv(*p4_type)) {
        return copy();
    }
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s for %s",
                      p4_type, dest_type, get_static_type());
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

void StructBase::bind(const z3::expr *bind_var, uint64_t offset) {
    auto var_width = get_width();
    // This struct is empty, we can not do anything here...
    if (var_width == 0) {
        return;
    }
    z3::expr tmp_var = state->get_z3_ctx()->bv_const(instance_name, var_width);
    if (bind_var == nullptr) {
        bind_var = &tmp_var;
        offset = var_width;
    }
    auto bit_idx = offset;
    for (auto type_tuple : members) {
        auto member_name = type_tuple.first;
        auto *member_var = type_tuple.second;
        if (auto *si = member_var->to_mut<StructBase>()) {
            si->bind(bind_var, bit_idx);
            bit_idx -= si->get_width();
        } else if (const auto *z3_var = member_var->to<Z3Bitvector>()) {
            auto var_width = z3_var->get_width();
            // TODO: Better casting
            auto extract_var =
                bind_var->extract(bit_idx - 1, bit_idx - var_width);
            if (z3_var->get_p4_type()->is<IR::Type_Boolean>()) {
                extract_var = extract_var > 0;
            }
            auto *bind_bv =
                new Z3Bitvector(state, z3_var->get_p4_type(), extract_var,
                                z3_var->bv_is_signed());
            update_member(member_name, bind_bv);
            bit_idx -= var_width;
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.",
                              member_var->get_static_type());
        }
    }
}
void StructBase::update_member(cstring name, P4Z3Instance *val) {
    // Assignments where we clearly know the header is invalid are invalid.
    // TODO: Handle the case with ambiguous ite valid.
    if (!valid.simplify().is_false()) {
        members.at(name) = val;
    }
}

z3::expr StructBase::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (other.is<ListInstance>()) {
        // We might be dealing with a list, flip and use the List implementation
        return other.operator==(*this);
    }
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
                               cstring name, uint64_t member_id)
    : StructBase(state, type, name, member_id) {
    auto flat_id = member_id;
    for (const auto *field : type->fields) {
        const IR::Type *resolved_type = state->resolve_type(field->type);
        auto *member_var = state->gen_instance(name, resolved_type, flat_id);
        if (auto *si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_width();
        } else if (auto *num_val = member_var->to_mut<Z3Bitvector>()) {
            width += num_val->get_width();
            flat_id += num_val->get_width();
            num_val->set_undefined();
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
            // We need to cast towards the member type
            const auto *dest_type = member_types.at(member_tuple.first);
            if (const auto *tb = dest_type->to<IR::Type_Bits>()) {
                auto cast_val =
                    z3::int2bv(tb->size, *z3_var->get_val()).simplify();
                auto invalid_var = state->gen_z3_expr(INVALID_LABEL, dest_type);
                auto valid_var = z3::ite(*tmp_valid, cast_val, invalid_var);
                z3_vars.emplace_back(name, valid_var);
            } else if (const auto *tb = dest_type->to<IR::Type_Varbits>()) {
                auto cast_val =
                    z3::int2bv(tb->size, *z3_var->get_val()).simplify();
                auto invalid_var = state->gen_z3_expr(INVALID_LABEL, dest_type);
                auto valid_var = z3::ite(*tmp_valid, cast_val, invalid_var);
                z3_vars.emplace_back(name, valid_var);
            } else {
                BUG("Unsupported int cast to %s.", dest_type);
            }
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
                               cstring name, uint64_t member_id)
    : StructInstance(state, type, name, member_id) {
    valid = state->get_z3_ctx()->bool_val(false);
    // When we first instantiate a header, all its members need to be invalid.
    propagate_validity(&valid);
    add_function("setValid0", [this](Visitor *visitor,
                                     const IR::Vector<IR::Argument> *args) {
        setValid(visitor, args);
    });
    add_function("setInvalid0", [this](Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args) {
        setInvalid(visitor, args);
    });
    add_function("isValid0", [this](Visitor *visitor,
                                    const IR::Vector<IR::Argument> *args) {
        isValid(visitor, args);
    });
}

HeaderInstance::HeaderInstance(const HeaderInstance &other)
    : StructInstance(other) {
    add_function("setValid0", [this](Visitor *visitor,
                                     const IR::Vector<IR::Argument> *args) {
        setValid(visitor, args);
    });
    add_function("setInvalid0", [this](Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args) {
        setInvalid(visitor, args);
    });
    add_function("isValid0", [this](Visitor *visitor,
                                    const IR::Vector<IR::Argument> *args) {
        isValid(visitor, args);
    });
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

void HeaderInstance::setValid(Visitor * /*visitor*/,
                              const IR::Vector<IR::Argument> * /*args*/) {
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::setInvalid(Visitor * /*visitor*/,
                                const IR::Vector<IR::Argument> * /*args*/) {
    set_valid(state->get_z3_ctx()->bool_val(false));
    propagate_validity(&valid);
    set_undefined();
    state->set_expr_result(new VoidResult());
}

void HeaderInstance::isValid(Visitor * /*visitor*/,
                             const IR::Vector<IR::Argument> * /*args*/) {
    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, valid));
}

void HeaderInstance::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr == nullptr) {
        cstring name = instance_name + "_valid";
        set_valid(state->get_z3_ctx()->bool_const(name));
        valid_expr = &valid;
    } else {
        set_valid(*valid_expr);
    }
    for (auto member_tuple : members) {
        auto *member = member_tuple.second;
        if (auto *z3_var = member->to_mut<StructBase>()) {
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
    auto valid_merge = z3::ite(cond, *then_struct->get_valid(), valid);
    set_valid(valid_merge);
    StructBase::merge(cond, then_var);
}

void HeaderInstance::set_list(std::vector<P4Z3Instance *> input_list) {
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
    StructBase::set_list(input_list);
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
                             cstring name, uint64_t member_id)
    : IndexableInstance(state, type, name, member_id),
      nextIndex(Z3Int(state, 0)), lastIndex(Z3Int(state, 0)),
      size(Z3Int(state, type->getSize())), int_size(type->getSize()),
      elem_type(state->resolve_type(type->elementType)) {
    auto flat_id = member_id;
    for (size_t idx = 0; idx < int_size; ++idx) {
        auto *member_var = state->gen_instance(name, elem_type, flat_id);
        if (auto *si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_width();
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.",
                              member_var->get_static_type());
        }
        cstring member_name = std::to_string(idx);
        insert_member(member_name, member_var);
        member_types.insert({member_name, elem_type});
    }
    add_function("push_front1", [this](Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args) {
        push_front(visitor, args);
    });
    add_function("pop_front1", [this](Visitor *visitor,
                                      const IR::Vector<IR::Argument> *args) {
        pop_front(visitor, args);
    });
}

StackInstance *StackInstance::copy() const { return new StackInstance(*this); }

StackInstance::StackInstance(const StackInstance &other)
    : IndexableInstance(other), nextIndex(other.nextIndex),
      lastIndex(other.lastIndex), size(other.size), int_size(other.int_size),
      elem_type(other.elem_type) {
    add_function("push_front1", [this](Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args) {
        push_front(visitor, args);
    });
    add_function("pop_front1", [this](Visitor *visitor,
                                      const IR::Vector<IR::Argument> *args) {
        pop_front(visitor, args);
    });
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
        return &size;
    }
    if (name == "nextIndex") {
        return &nextIndex;
    }
    if (name == "lastIndex") {
        return &lastIndex;
    }
    if (name == "next") {
        // TODO: Move this into extract as functionality
        lastIndex = nextIndex;
        // nextIndex = Z3Int(state, *nextIndex.get_val() + 1);
        return get_member(*lastIndex.get_val());
    }
    if (name == "last") {
        return get_member(*lastIndex.get_val());
    }
    return StructBase::get_member(name);
}

const IR::Type *StackInstance::get_member_type(cstring /*name*/) const {
    return elem_type;
}

void StackInstance::update_member(cstring name, P4Z3Instance *val) {
    if (name == "size" || name == "nextIndex" || name == "lastIndex" ||
        name == "next") {
        return;
    }
    members.at(name) = val;
}

P4Z3Instance *StackInstance::get_member(const z3::expr &index) const {
    auto val = index.simplify();
    std::string val_str;
    if (val.is_numeral(val_str, 0)) {
        return StructBase::get_member(val_str);
    }
    // We create a new header that we return
    // This header is the merge of all the sub headers of this stack
    auto *base_hdr = state->gen_instance(UNDEF_LABEL, elem_type);
    // Sometimes the index bitvector is so small, it does not exceed the header
    // size. So we have to make sure max_idx is computed correctly.
    auto bv_size = val.get_sort().bv_size();
    size_t max = (1 << bv_size);
    auto max_idx = std::min<size_t>(max, int_size);
    for (size_t idx = 0; idx < max_idx; ++idx) {
        cstring member_name = std::to_string(idx);
        const auto *hdr = get_member(member_name);
        auto z3_int = state->get_z3_ctx()->num_val(idx, val.get_sort());
        base_hdr->merge(val == z3_int, *hdr);
    }
    return base_hdr;
}

void StackInstance::push_front(Visitor *visitor,
                               const IR::Vector<IR::Argument> *args) {
    if (args->size() != 1) {
        error("Expected one argument for push_front, received %s",
              args->size());
    }
    visitor->visit(args->at(0)->expression);
    const auto *numeric_val = state->get_expr_result<NumericVal>();
    const auto z3_push_size = numeric_val->get_val()->simplify();
    auto int_push_size = z3_push_size.get_numeral_uint64();
    // TODO: Checks
    for (size_t idx = 0; idx < int_push_size; ++idx) {
        // Check if we are pushing beyond the stack size
        if (idx >= int_size) {
            break;
        }
        auto *member = get_member(std::to_string(idx));
        auto *hdr = member->to_mut<HeaderInstance>();
        hdr->setInvalid(visitor, {});
    }
    nextIndex = Z3Int(state, (*nextIndex.get_val() + z3_push_size).simplify());
    if ((nextIndex > size).is_true()) {
        nextIndex = size;
    }
    lastIndex = nextIndex;
}
void StackInstance::pop_front(Visitor *visitor,
                              const IR::Vector<IR::Argument> *args) {
    if (args->size() != 1) {
        error("Expected one argument for push_front, received %s",
              args->size());
    }
    visitor->visit(args->at(0)->expression);
    const auto *numeric_val = state->get_expr_result<NumericVal>();
    const auto z3_pop_size = numeric_val->get_val()->simplify();
    auto int_pop_size = z3_pop_size.get_numeral_uint64();
    auto last_range = int_pop_size > int_size ? 0 : int_size - int_pop_size;
    for (size_t idx = last_range; idx < int_size; ++idx) {
        // Check if we are pushing beyond the stack size
        if (idx >= int_size) {
            break;
        }
        auto *member = get_member(std::to_string(idx));
        auto *hdr = member->to_mut<HeaderInstance>();
        hdr->setInvalid(visitor, {});
    }
    if ((*nextIndex.get_val() < z3_pop_size).is_true()) {
        nextIndex = Z3Int(state, state->get_z3_ctx()->int_val(0));
    } else {
        nextIndex =
            Z3Int(state, (*nextIndex.get_val() - z3_pop_size).simplify());
    }
    lastIndex = nextIndex;
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
                                         cstring name, uint64_t member_id)
    : StructBase(state, type, name, member_id) {
    auto flat_id = member_id;
    for (const auto *field : type->fields) {
        const IR::Type *resolved_type = state->resolve_type(field->type);
        if (resolved_type->is<IR::Type_Header>()) {
            auto *member_var = state->gen_instance(
                name + std::to_string(flat_id), resolved_type, flat_id);
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
    add_function("isValid0", [this](Visitor *visitor,
                                    const IR::Vector<IR::Argument> *args) {
        isValid(visitor, args);
    });
}

HeaderUnionInstance::HeaderUnionInstance(const HeaderUnionInstance &other)
    : StructBase(other) {
    add_function("isValid0", [this](Visitor *visitor,
                                    const IR::Vector<IR::Argument> *args) {
        isValid(visitor, args);
    });
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

void HeaderUnionInstance::isValid(Visitor * /*visitor*/,
                                  const IR::Vector<IR::Argument> * /*args*/) {
    state->set_expr_result(new Z3Bitvector(state, &BOOL_TYPE, get_valid()));
}

HeaderUnionInstance *HeaderUnionInstance::copy() const {
    return new HeaderUnionInstance(*this);
}

void HeaderUnionInstance::update_validity(const HeaderInstance * /*child*/,
                                          const z3::expr &valid_val) {
    for (auto &member : members) {
        auto *hi = member.second->to_mut<HeaderInstance>();
        BUG_CHECK(hi, "Unexpected instance %s", member.second->to_string());
        const auto *old_valid = hi->get_valid();
        // This is kind of stupid but works,
        // I have no means to check child equality yet
        // TODO: Test this...
        hi->valid = z3::ite(valid_val, state->get_z3_ctx()->bool_val(false),
                            *old_valid);
    }
}

/***
===============================================================================
EnumBase
===============================================================================
***/

EnumBase::EnumBase(P4State *state, const IR::Type *type, cstring name,
                   uint64_t member_id)
    : StructBase(state, type, name, member_id),
      ValContainer(state->gen_z3_expr(UNDEF_LABEL, &P4_STD_BIT_TYPE)) {}

std::vector<std::pair<cstring, z3::expr>>
EnumBase::get_z3_vars(cstring prefix, const z3::expr *valid_expr) const {
    // TODO: Clean this up and split it
    const z3::expr *tmp_valid = nullptr;
    if (valid_expr != nullptr) {
        tmp_valid = valid_expr;
    } else {
        tmp_valid = &valid;
    }
    cstring name = instance_name;
    if (prefix.size() != 0) {
        name = prefix + "." + name;
    }
    auto invalid_var = state->gen_z3_expr(INVALID_LABEL, member_type);
    auto valid_var = z3::ite(*tmp_valid, val, invalid_var);
    std::vector<std::pair<cstring, z3::expr>> z3_vars = {{name, valid_var}};
    return z3_vars;
}

void EnumBase::add_enum_member(cstring error_name) {
    insert_member(error_name, new Z3Bitvector(state, member_type, val));
}

void EnumBase::set_undefined() {
    val = state->gen_z3_expr(UNDEF_LABEL, member_type);
}

void EnumBase::bind(const z3::expr *bind_var, uint64_t offset) {
    if (bind_var != nullptr) {
        auto var_width = get_width();
        val = bind_var->extract(offset - 1, offset - var_width);
    }
}

z3::expr EnumBase::operator==(const P4Z3Instance &other) const {
    if (const auto *other_numeric = other.to<Z3Bitvector>()) {
        auto other_val = *other_numeric->get_val();
        auto cast_val = pure_bv_cast(val, other_val.get_sort());
        return cast_val == other_val;
    }
    if (const auto *other_numeric = other.to<Z3Int>()) {
        auto cast_val = pure_bv_cast(*other_numeric->get_val(), val.get_sort());
        return val == cast_val;
    }
    if (const auto *other_enum = other.to<EnumBase>()) {
        return val == *other_enum->get_val();
    }
    P4C_UNIMPLEMENTED("Comparing a enum base to %s is not supported.",
                      other.get_static_type());
}

z3::expr EnumBase::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

P4Z3Instance *EnumBase::operator&(const P4Z3Instance &other) const {
    return Z3Bitvector(state, &P4_STD_BIT_TYPE, val, false) & other;
}
P4Z3Instance *EnumBase::operator|(const P4Z3Instance &other) const {
    return Z3Bitvector(state, &P4_STD_BIT_TYPE, val, false) | other;
}

void EnumBase::set_enum_val(const z3::expr &enum_input) { val = enum_input; }

void EnumBase::merge(const z3::expr &cond, const P4Z3Instance &then_expr) {
    const auto *then_enum = then_expr.to<EnumBase>();
    BUG_CHECK(then_enum, "Unsupported merge class.");
    val = z3::ite(cond, *then_enum->get_val(), val);
}

EnumBase::EnumBase(const EnumBase &other)
    : StructBase(other), ValContainer(other.val),
      member_type(other.member_type) {}

/***
===============================================================================
EnumInstance
===============================================================================
***/

EnumInstance::EnumInstance(P4State *p4_state, const IR::Type_Enum *type,
                           cstring name, uint64_t member_id)
    : EnumBase(p4_state, type, name, member_id) {
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

EnumInstance *EnumInstance::instantiate(const NumericVal &enum_val) const {
    auto *enum_copy = new EnumInstance(*this);
    auto current_sort = val.get_sort();
    enum_copy->set_enum_val(pure_bv_cast(*enum_val.get_val(), current_sort));
    return enum_copy;
}

EnumInstance::EnumInstance(const EnumInstance &other) : EnumBase(other) {}

EnumInstance &EnumInstance::operator=(const EnumInstance &other) {
    if (this == &other) {
        return *this;
    }
    *this = EnumInstance(other);
    return *this;
}

/***
===============================================================================
ErrorInstance
==================================d=============================================
***/

ErrorInstance::ErrorInstance(P4State *p4_state, const IR::Type_Error *type,
                             cstring name, uint64_t member_id)
    : EnumBase(p4_state, type, name, member_id) {
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

ErrorInstance *ErrorInstance::instantiate(const NumericVal &enum_val) const {
    auto *enum_copy = new ErrorInstance(*this);
    auto current_sort = val.get_sort();
    enum_copy->set_enum_val(pure_bv_cast(*enum_val.get_val(), current_sort));
    return enum_copy;
}

/***
===============================================================================
SerEnumInstance
===============================================================================
***/

SerEnumInstance::SerEnumInstance(
    P4State *p4_state,
    const ordered_map<cstring, P4Z3Instance *> &input_members,
    const IR::Type_SerEnum *type, cstring name, uint64_t member_id)
    : EnumBase(p4_state, type, name, member_id) {
    members.clear();
    members.insert(input_members.begin(), input_members.end());
    const auto *resolved_type = state->resolve_type(type->type);
    val = state->gen_z3_expr(UNDEF_LABEL, resolved_type);
    if (const auto *tb = resolved_type->to<IR::Type_Bits>()) {
        member_type = tb;
        width = tb->size;
    } else {
        P4C_UNIMPLEMENTED("Type %s not supported for SerEnum!",
                          type->type->node_type_name());
    }
}

SerEnumInstance *SerEnumInstance::copy() const {
    return new SerEnumInstance(*this);
}

SerEnumInstance *
SerEnumInstance::instantiate(const NumericVal &enum_val) const {
    auto *enum_copy = new SerEnumInstance(*this);
    auto current_sort = val.get_sort();
    enum_copy->set_enum_val(pure_bv_cast(*enum_val.get_val(), current_sort));
    return enum_copy;
}

P4Z3Instance *SerEnumInstance::operator&(const P4Z3Instance &other) const {
    // TODO: Kind of crazy, should I really do this?
    const auto *tb = p4_type->checkedTo<IR::Type_SerEnum>()
                         ->type->checkedTo<IR::Type_Bits>();
    return Z3Bitvector(state, tb, val, tb->isSigned) & other;
}
P4Z3Instance *SerEnumInstance::operator|(const P4Z3Instance &other) const {
    // TODO: Kind of crazy, should I really do this?
    const auto *tb = p4_type->checkedTo<IR::Type_SerEnum>()
                         ->type->checkedTo<IR::Type_Bits>();
    return Z3Bitvector(state, tb, val, tb->isSigned) | other;
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
            if (param->isOptional() || param->defaultValue != nullptr) {
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

P4Z3Instance *ExternInstance::cast_allocate(const IR::Type *dest_type) const {
    // There is only rudimentary casting support, just copy for now
    // TODO: Make this proper and think about equality here...
    return copy();
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s for %s",
                      p4_type, dest_type, get_static_type());
}

/***
===============================================================================
ListInstance
===============================================================================
***/
ListInstance::ListInstance(P4State *state,
                           const std::vector<P4Z3Instance *> &val_list,
                           const IR::Type *type_list)
    : StructBase(state, type_list, "", 0) {
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
                           cstring name, uint64_t member_id)
    : StructBase(state, list_type, name, member_id) {
    auto flat_id = member_id;
    for (size_t idx = 0; idx < list_type->components.size(); ++idx) {
        const auto *type = list_type->components[idx];
        cstring name = std::to_string(idx);
        insert_member(name, state->gen_instance(name, type));
        member_types.insert({name, type});
        flat_id++;
    }
}

P4Z3Instance *ListInstance::cast_allocate(const IR::Type *dest_type) const {
    if (dest_type->equiv(*p4_type)) {
        return copy();
    }
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
    return new ListInstance(state, get_val_list(), p4_type);
}

std::vector<P4Z3Instance *> ListInstance::get_val_list() const {
    std::vector<P4Z3Instance *> val_list;
    for (const auto &member : members) {
        val_list.push_back(member.second);
    }
    return val_list;
}

void unroll_list(const StructBase *input_struct,
                 std::vector<P4Z3Instance *> *target_list) {
    for (const auto &member : *input_struct->get_member_map()) {
        auto *member_instance = member.second;
        if (const auto *sb = member_instance->to<StructBase>()) {
            unroll_list(sb, target_list);
        } else {
            target_list->push_back(member_instance);
        }
    }
}

z3::expr ListInstance::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (const auto *other_struct = other.to<StructBase>()) {
        std::vector<P4Z3Instance *> member_list;
        unroll_list(this, &member_list);
        std::vector<P4Z3Instance *> other_list;
        unroll_list(other_struct, &other_list);
        if (member_list.size() != other_list.size()) {
            return state->get_z3_ctx()->bool_val(false);
        }
        // From here on out we assume equal member size.
        for (size_t idx = 0; idx < member_list.size(); ++idx) {
            const auto *member_val = member_list.at(idx);
            const auto *other_val = other_list.at(idx);
            is_eq = is_eq && (member_val->operator==(*other_val));
        }
        return is_eq;
    }
    if (other.is<NumericVal>()) {
        if (members.size() != 1) {
            return state->get_z3_ctx()->bool_val(false);
        }
        // Compare the first element in the list
        return *members.begin()->second == other;
    }
    P4C_UNIMPLEMENTED("Comparing a ListInstance to %s is not supported.",
                      other.get_static_type());
}

z3::expr ListInstance::operator!=(const P4Z3Instance &other) const {
    return !(*this == other);
}

/***
===============================================================================
TupleInstance
===============================================================================
***/

TupleInstance::TupleInstance(P4State *state, const IR::Type_Tuple *type,
                             cstring name, uint64_t member_id)
    : IndexableInstance(state, type, name, member_id) {
    size_t idx = 0;
    for (const auto &field_type : type->components) {
        const IR::Type *resolved_type = state->resolve_type(field_type);
        auto *member_var =
            state->gen_instance(name, resolved_type, member_id + idx);
        cstring name = std::to_string(idx);
        insert_member(name, member_var);
        member_types.insert({name, resolved_type});
        idx++;
    }
}

TupleInstance *TupleInstance::copy() const { return new TupleInstance(*this); }

P4Z3Instance *TupleInstance::get_member(const z3::expr &index) const {
    auto val = index.simplify();
    std::string val_str;
    if (val.is_numeral(val_str, 0)) {
        return StructBase::get_member(val_str);
    }
    P4C_UNIMPLEMENTED("Runtime indices not supported for %s",
                      get_static_type());
}

/***
===============================================================================
ControlInstance
===============================================================================
***/

ControlInstance::ControlInstance(P4State *state, const IR::Type *decl,
                                 const VarMap &input_const_args)
    : P4Z3Instance(decl), state(state) {
    resolved_const_args.insert(input_const_args.begin(),
                               input_const_args.end());
    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    const IR::ParameterList *const_params = nullptr;
    if (const auto *ctrl = decl->to<IR::P4Control>()) {
        params = ctrl->getApplyParameters();
        type_params = ctrl->getTypeParameters();
        const_params = ctrl->getConstructorParameters();
    } else if (const auto *parser = decl->to<IR::P4Parser>()) {
        params = parser->getApplyParameters();
        type_params = parser->getTypeParameters();
        const_params = parser->getConstructorParameters();
    } else {
        BUG("Unsupported control instance %s.", decl->node_type_name());
    }
    auto num_params = 0;
    auto num_optional_params = 0;
    for (const auto *param : *params) {
        if (param->isOptional() || param->defaultValue != nullptr) {
            num_optional_params += 1;
        } else {
            num_params += 1;
        }
    }
    for (auto idx = 0; idx <= num_optional_params; ++idx) {
        cstring apply_str = "apply" + std::to_string(num_params + idx);
        add_function(apply_str, [this](Visitor *visitor,
                                       const IR::Vector<IR::Argument> *args) {
            apply(visitor, args);
        });
    }
    for (const auto &arg : resolved_const_args) {
        const auto arg_name = arg.first;
        const auto *arg_type = arg.second.second;
        const auto *param = const_params->getParameter(arg_name);
        CHECK_NULL(param);
        if (const auto *tn = param->type->to<IR::Type_Name>()) {
            if (type_params->getDeclByName(tn->path->name.name) != nullptr) {
                local_type_map.emplace(tn->path->name.name, arg_type);
            }
        }
    }
}

void ControlInstance::apply(Visitor *visitor,
                            const IR::Vector<IR::Argument> *args) {
    const IR::ParameterList *params = nullptr;
    const IR::TypeParameters *type_params = nullptr;
    IR::IndexedVector<IR::Declaration> local_decls;
    const IR::BlockStatement *body = nullptr;
    IR::IndexedVector<IR::ParserState> parser_states;
    if (const auto *control = p4_type->to<IR::P4Control>()) {
        params = control->getApplyParameters();
        type_params = control->getApplyMethodType()->getTypeParameters();
        local_decls = control->controlLocals;
        body = control->body;
    } else if (const auto *parser = p4_type->to<IR::P4Parser>()) {
        params = parser->getApplyParameters();
        type_params = parser->getApplyMethodType()->getTypeParameters();
        local_decls = parser->parserLocals;
        parser_states = parser->states;
    }
    for (auto &local_type : local_type_map) {
        state->add_type(local_type.first, local_type.second);
    }
    CHECK_NULL(params);
    const ParamInfo param_info = {*params, *args, *type_params, {}};
    state->copy_in(visitor, param_info);
    for (const auto &const_arg : resolved_const_args) {
        state->declare_var(const_arg.first, const_arg.second.first,
                           const_arg.second.second);
    }
    for (const auto *local_decl : local_decls) {
        visitor->visit(local_decl);
    }
    if (!parser_states.empty()) {
        for (const auto &parser_state : parser_states) {
            state->declare_static_decl(parser_state->name.name,
                                       new P4Declaration(parser_state));
        }
        visitor->visit(state->get_static_decl("start")->get_decl());
    }
    if (body != nullptr) {
        visitor->visit(body);
    }
    state->copy_out();
}

std::map<cstring, const IR::Type *>
get_type_mapping(const IR::ParameterList *src_params,
                 const IR::TypeParameters *src_type_params,
                 const IR::ParameterList *dest_params) {
    std::map<cstring, const IR::Type *> type_mapping;
    auto dest_params_size = dest_params->size();
    for (size_t idx = 0; idx < src_params->size(); ++idx) {
        // Ignore optional parameters.
        if (idx >= dest_params_size) {
            continue;
        }
        const auto *src_param = src_params->getParameter(idx);
        if (const auto *tn = src_param->type->to<IR::Type_Name>()) {
            auto src_type_name = tn->path->name.name;
            if (src_type_params->getDeclByName(src_type_name) != nullptr) {
                const auto *dst_param = dest_params->getParameter(idx);
                type_mapping.emplace(src_type_name, dst_param->type);
            }
        }
    }
    return type_mapping;
}

P4Z3Instance *ControlInstance::cast_allocate(const IR::Type *dest_type) const {
    // There is only rudimentary casting support, just copy for now
    // TODO: Make this proper and think about equality here...
    if (const auto *control = p4_type->to<IR::P4Control>()) {
        if (const auto *control_dst_type = dest_type->to<IR::Type_Control>()) {
            const auto *src_params = control->getApplyParameters();
            const auto *src_type_params = control->getTypeParameters();
            const auto type_mapping =
                get_type_mapping(src_params, src_type_params,
                                 control_dst_type->getApplyParameters());
            TypeModifier type_modifier(&type_mapping);
            const auto *cast_type =
                p4_type->clone()->apply(type_modifier)->checkedTo<IR::Type>();
            return new ControlInstance(state, cast_type, resolved_const_args);
        }
    }
    if (const auto *parser = p4_type->to<IR::P4Parser>()) {
        if (const auto *parser_dst_type = dest_type->to<IR::Type_Parser>()) {
            const auto *src_params = parser->getApplyParameters();
            const auto *src_type_params = parser->getTypeParameters();
            const auto type_mapping =
                get_type_mapping(src_params, src_type_params,
                                 parser_dst_type->getApplyParameters());
            TypeModifier type_modifier(&type_mapping);
            const auto *cast_type =
                p4_type->clone()->apply(type_modifier)->checkedTo<IR::Type>();
            return new ControlInstance(state, cast_type, resolved_const_args);
        }
    }
    P4C_UNIMPLEMENTED("Unsupported cast from type %s to type %s for %s",
                      p4_type, dest_type, get_static_type());
}

}  // namespace TOZ3
