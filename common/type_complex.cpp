#include "type_complex.h"

#include <cstdio>
#include <utility>

#include "state.h"

namespace TOZ3_V2 {

StructBase::StructBase(P4State *state, const IR::Type *type, uint64_t member_id)
    : state(state), member_id(member_id),
      valid(state->get_z3_ctx()->bool_val(true)), p4_type(type) {
    width = 0;
}

StructBase::StructBase(const StructBase &other)
    : P4Z3Instance(other), valid(other.valid) {
    width = other.width;
    state = other.state;
    valid = other.valid;
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

void StructBase::merge(const z3::expr &cond, const P4Z3Instance &then_var) {
    auto then_struct = then_var.to<StructBase>();

    if (!then_struct) {
        BUG("Unsupported merge class.");
    }

    for (auto member_tuple : members) {
        cstring member_name = member_tuple.first;
        auto then_var = member_tuple.second;
        auto else_var = then_struct->get_const_member(member_name);
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

void StructBase::propagate_validity(const z3::expr *valid_expr) {
    if (valid_expr) {
        valid = *valid_expr;
    }
    for (auto member_tuple : members) {
        auto member = member_tuple.second;
        if (auto z3_var = member->to_mut<StructBase>()) {
            z3_var->propagate_validity(valid_expr);
        }
    }
}

z3::expr StructBase::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (other.is<StructBase>()) {
        for (auto member_tuple : members) {
            auto member_name = member_tuple.first;
            auto member_val = member_tuple.second;
            auto other_val = other.get_member(member_name);
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

StructInstance::StructInstance(P4State *state, const IR::Type_StructLike *type,
                               uint64_t member_id)
    : StructBase(state, type, member_id) {
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
            auto valid_var =
                z3::ite(*tmp_valid, *z3_var->get_val(), invalid_var);
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
                z3::int2bv(dest_type->width_bits(), *z3_var->get_val())
                    .simplify();
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
    : StructBase(other) {}

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

HeaderInstance::HeaderInstance(P4State *state, const IR::Type_Header *type,
                               uint64_t member_id)
    : StructInstance(state, type, member_id) {
    valid = state->get_z3_ctx()->bool_val(false);
    member_functions["setValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setValid(visitor, args);
        });
    member_functions["setInvalid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setInvalid(visitor, args);
        });
    member_functions["isValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        });
}

HeaderInstance::HeaderInstance(const HeaderInstance &other)
    : StructInstance(other) {
    member_functions["setValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setValid(visitor, args);
        });
    member_functions["setInvalid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setInvalid(visitor, args);
        });
    member_functions["isValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        });
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
    member_functions["setValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setValid(visitor, args);
        });
    member_functions["setInvalid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            setInvalid(visitor, args);
        });
    member_functions["isValid0"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            isValid(visitor, args);
        });
    return *this;
}

z3::expr HeaderInstance::operator==(const P4Z3Instance &other) const {
    auto is_eq = state->get_z3_ctx()->bool_val(true);
    if (auto other_hdr = other.to<HeaderInstance>()) {
        auto other_is_valid = *other_hdr->get_valid();
        for (auto member_tuple : members) {
            auto member_name = member_tuple.first;
            auto member_val = member_tuple.second;
            auto other_val = other.get_member(member_name);
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

void HeaderInstance::set_valid(const z3::expr &valid_val) { valid = valid_val; }
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

void HeaderInstance::merge(const z3::expr &cond, const P4Z3Instance &then_var) {
    auto then_struct = then_var.to<HeaderInstance>();

    if (!then_struct) {
        P4C_UNIMPLEMENTED("Unsupported merge class.");
    }
    StructBase::merge(cond, then_var);
    auto valid_merge = z3::ite(cond, *then_struct->get_valid(), valid);
    set_valid(valid_merge);
}
void HeaderInstance::set_list(std::vector<P4Z3Instance *> input_list) {
    StructBase::set_list(input_list);
    set_valid(state->get_z3_ctx()->bool_val(true));
    propagate_validity(&valid);
}

StackInstance::StackInstance(P4State *state, const IR::Type_Stack *type,
                             uint64_t member_id)
    : StructBase(state, type, member_id), nextIndex(Z3Int(state, 0)),
      lastIndex(Z3Int(state, 0)), size(Z3Int(state, type->getSize())),
      int_size(type->getSize()), elem_type(type->elementType) {
    auto flat_id = member_id;
    const IR::Type *resolved_type = state->resolve_type(type->elementType);
    for (size_t idx = 0; idx < int_size; ++idx) {
        cstring name = std::to_string(flat_id);
        cstring member_name = std::to_string(idx);
        auto member_var = state->gen_instance(name, resolved_type, flat_id);
        if (auto si = member_var->to_mut<StructBase>()) {
            width += si->get_width();
            flat_id += si->get_member_map()->size();
        } else {
            P4C_UNIMPLEMENTED("Type \"%s\" not supported!.",
                              member_var->get_static_type());
        }
        insert_member(member_name, member_var);
        member_types.insert({member_name, resolved_type});
    }
    member_functions["push_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            push_front(visitor, args);
        });
    member_functions["pop_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            pop_front(visitor, args);
        });
}

StackInstance *StackInstance::copy() const { return new StackInstance(*this); }

StackInstance::StackInstance(const StackInstance &other)
    : StructBase(other), nextIndex(other.nextIndex), lastIndex(other.lastIndex),
      size(other.size), int_size(other.int_size), elem_type(other.elem_type) {
    member_functions["push_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            push_front(visitor, args);
        });
    member_functions["pop_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            pop_front(visitor, args);
        });
}

StackInstance &StackInstance::operator=(const StackInstance &other) {
    // TODO: Clean this up and call super functions
    if (this == &other) {
        return *this;
    }
    width = other.width;
    state = other.state;
    p4_type = other.p4_type;
    member_types = other.member_types;
    for (auto value_tuple : other.members) {
        cstring name = value_tuple.first;
        auto member_cpy = value_tuple.second->copy();
        insert_member(name, member_cpy);
    }
    nextIndex = other.nextIndex;
    lastIndex = other.lastIndex;
    size = other.size;
    int_size = other.int_size;
    elem_type = other.elem_type;
    member_functions["push_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            push_front(visitor, args);
        });
    member_functions["pop_front1"] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            pop_front(visitor, args);
        });
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

P4Z3Instance *StackInstance::get_member(const P4Z3Instance *index) const {
    auto z3_expr = index->to<NumericVal>();
    BUG_CHECK(z3_expr, "Index of type %s not implemented for stacks.",
              index->get_static_type());
    auto val = z3_expr->get_val()->simplify();
    std::string val_str;
    if (val.is_numeral(val_str, 0)) {
        return get_member(val_str);
    } else {
        // We create a new header that we return
        // This header is the merge of all the sub headers of this stack
        auto base_hdr = state->gen_instance("undefined", elem_type);
        for (size_t idx = 0; idx < int_size; ++idx) {
            cstring member_name = std::to_string(idx);
            auto hdr = get_member(member_name);
            auto z3_int = state->get_z3_ctx()->num_val(idx, val.get_sort());
            base_hdr->merge(val == z3_int, *hdr);
        }
        return base_hdr;
    }
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
        auto member = member_tuple.second;
        if (auto z3_var = member->to<HeaderInstance>()) {
            auto z3_sub_vars = z3_var->get_z3_vars(name, valid_expr);
            z3_vars.insert(z3_vars.end(), z3_sub_vars.begin(),
                           z3_sub_vars.end());
        } else {
            BUG("Var is neither type z3::expr nor HeaderInstance!");
        }
    }
    return z3_vars;
}

EnumInstance::EnumInstance(P4State *p4_state, const IR::Type_Enum *type,
                           uint64_t ext_member_id)
    : StructBase(p4_state, type, ext_member_id), p4_type(type) {
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
    : StructBase(p4_state, type, ext_member_id), p4_type(type) {
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

ExternInstance::ExternInstance(P4State *state, const IR::Type_Extern *type)
    : state(state), p4_type(type) {
    for (auto method : type->methods) {
        // FIXME: Overloading uses num of parameters, it should use types
        cstring overloaded_name = method->getName().name;
        auto num_params = 0;
        auto num_optional_params = 0;
        for (auto param : method->getParameters()->parameters) {
            if (param->isOptional()) {
                num_optional_params += 1;
            } else {
                num_params += 1;
            }
        }
        auto decl = new P4Declaration(method);
        for (auto idx = 0; idx <= num_optional_params; ++idx) {
            // The IR has bizarre side effects when storing pointers in a map
            // FIXME: Think about how to simplify this, maybe use their vector
            cstring name = overloaded_name + std::to_string(num_params + idx);
            methods.insert({name, decl});
        }
    }
}

P4Z3Instance *ListInstance::cast_allocate(const IR::Type *dest_type) const {
    auto instance = state->gen_instance("list", dest_type);
    auto struct_instance = instance->to_mut<StructBase>();
    if (struct_instance == nullptr) {
        P4C_UNIMPLEMENTED("Unsupported type %s for ListInstance.",
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
    member_functions[apply_str] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            apply(visitor, args);
        });
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
    member_functions[apply_str] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            apply(visitor, args);
        });
}

void P4TableInstance::apply(Visitor *visitor,
                            const IR::Vector<IR::Argument> *args) {
    auto table_decl = decl->to<IR::P4Table>();
    CHECK_NULL(table_decl);
    auto params = table_decl->getApplyParameters();
    state->copy_in(visitor, params, args);
    visitor->visit(decl);
    state->copy_out();
}

DeclarationInstance::DeclarationInstance(P4State *state,
                                         const IR::Type_Declaration *decl)
    : state(state), decl(decl) {
    cstring apply_str = "apply";
    if (auto ctrl = decl->to<IR::P4Control>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    } else if (auto ctrl = decl->to<IR::P4Parser>()) {
        apply_str += std::to_string(ctrl->getApplyParameters()->size());
    }
    member_functions[apply_str] = new FunctionWrapper(
        [this](Visitor *visitor, const IR::Vector<IR::Argument> *args) {
            apply(visitor, args);
        });
}

void DeclarationInstance::apply(Visitor *visitor,
                                const IR::Vector<IR::Argument> *args) {
    const IR::ParameterList *params = nullptr;
    if (auto control = decl->to<IR::P4Control>()) {
        params = control->getApplyParameters();
    } else if (auto parser = decl->to<IR::P4Parser>()) {
        params = parser->getApplyParameters();
    }
    CHECK_NULL(params);
    state->copy_in(visitor, params, args);
    visitor->visit(decl);
    state->copy_out();
}

} // namespace TOZ3_V2
