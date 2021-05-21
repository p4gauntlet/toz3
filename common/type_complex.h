#ifndef TOZ3_COMMON_TYPE_COMPLEX_H_
#define TOZ3_COMMON_TYPE_COMPLEX_H_
#include <cstdio>

#include <map>      // std::map
#include <string>   // std::to_string
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_simple.h"

namespace TOZ3 {

class FunctionClass {
 private:
    std::map<cstring, P4Z3Function> member_functions;

 public:
    virtual FunOrMethod get_function(cstring name) const {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }
    virtual void add_function(cstring name, P4Z3Function function) {
        member_functions.emplace(name, function);
    }
};

class StructBase : public P4Z3Instance {
 protected:
    P4State *state;
    ordered_map<cstring, P4Z3Instance *> members;
    std::map<cstring, const IR::Type *> member_types;
    uint64_t width;
    z3::expr valid;
    cstring instance_name;

 public:
    StructBase(P4State *state, const IR::Type *type, cstring name,
               uint64_t member_id);

    uint64_t get_width() const { return width; }

    const P4Z3Instance *get_const_member(const cstring name) const {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    P4Z3Instance *get_member(const cstring name) const override {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    virtual const IR::Type *get_member_type(cstring name) const {
        auto it = member_types.find(name);
        if (it != member_types.end()) {
            return it->second;
        }
        BUG("Name %s not found in member type map of %s.", name,
            get_static_type());
    }

    virtual void update_member(cstring name, P4Z3Instance *val) {
        members.at(name) = val;
    }
    void insert_member(cstring name, P4Z3Instance *val) {
        members.emplace(name, val);
    }
    const ordered_map<cstring, P4Z3Instance *> *get_member_map() const {
        return &members;
    }
    void set_undefined() override;
    virtual void propagate_validity(const z3::expr *valid_expr = nullptr);
    virtual void bind(const z3::expr *bind_var = nullptr, uint64_t offset = 0);
    virtual void set_list(std::vector<P4Z3Instance *>);

    // copy constructor
    StructBase(const StructBase &other);
    // overload = operator
    StructBase &operator=(const StructBase &other);
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class StructInstance : public StructBase {
    using StructBase::StructBase;

 public:
    StructInstance(P4State *state, const IR::Type_StructLike *type,
                   cstring name, uint64_t member_id);
    StructInstance *copy() const override;
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "StructInstance"; }
    cstring to_string() const override {
        cstring ret = "StructInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    // copy constructor
    StructInstance(const StructInstance &other);
    // overload = operator
    StructInstance &operator=(const StructInstance &other);
};

class HeaderInstance : public StructInstance, public FunctionClass {
    using StructInstance::StructInstance;
    // HeaderUnionInstances are friend classes because they need direct var
    // access outside the API
    friend HeaderUnionInstance;

 private:
    HeaderUnionInstance *parent_union = nullptr;

 public:
    HeaderInstance(P4State *state, const IR::Type_Header *type, cstring name,
                   uint64_t member_id);
    void set_valid(const z3::expr &valid_val);
    const z3::expr *get_valid() const;
    void setValid(Visitor *visitor, const IR::Vector<IR::Argument> *args);
    void setInvalid(Visitor *visitor, const IR::Vector<IR::Argument> *args);
    void isValid(Visitor *visitor, const IR::Vector<IR::Argument> *args);
    void propagate_validity(const z3::expr *valid_expr = nullptr) override;
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    void set_list(std::vector<P4Z3Instance *> input_list) override;
    void bind_to_union(HeaderUnionInstance *union_parent);

    cstring get_static_type() const override { return "HeaderInstance"; }
    cstring to_string() const override {
        cstring ret = "HeaderInstance(";
        ret += "valid: " + valid.to_string() + ", ";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    // copy constructor
    HeaderInstance *copy() const override;
    HeaderInstance(const HeaderInstance &other);
    // overload = operator
    HeaderInstance &operator=(const HeaderInstance &other);
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class IndexableInstance : public StructBase {
    using StructBase::StructBase;

 public:
    virtual P4Z3Instance *get_member(const z3::expr &index) const = 0;
    virtual size_t get_int_size() const = 0;
};

class StackInstance : public IndexableInstance, public FunctionClass {
 private:
    mutable Z3Int nextIndex;
    mutable Z3Int lastIndex;
    mutable Z3Int size;
    size_t int_size;
    const IR::Type *elem_type;

 public:
    explicit StackInstance(P4State *state, const IR::Type_Stack *type,
                           cstring name, uint64_t member_id);

    P4Z3Instance *get_member(const z3::expr &index) const override;
    P4Z3Instance *get_member(cstring name) const override;
    const IR::Type *get_member_type(cstring name) const override;
    void update_member(cstring name, P4Z3Instance *val) override;
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "StackInstance"; }
    cstring to_string() const override {
        cstring ret = "StackInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    size_t get_int_size() const override { return int_size; }
    void push_front(Visitor *, const IR::Vector<IR::Argument> *);
    void pop_front(Visitor *, const IR::Vector<IR::Argument> *);

    // copy constructor
    StackInstance *copy() const override;
    StackInstance(const StackInstance &other);
    // overload = operator
    StackInstance &operator=(const StackInstance &other);
};

class TupleInstance : public IndexableInstance {
 public:
    TupleInstance(P4State *state, const IR::Type_Tuple *type, cstring name,
                  uint64_t member_id);

    cstring get_static_type() const override { return "TupleInstance"; }
    cstring to_string() const override {
        cstring ret = "TupleInstance(";
        ret += ")";
        return ret;
    }
    TupleInstance *copy() const override;
    size_t get_int_size() const override { return members.size(); }
    P4Z3Instance *get_member(const z3::expr &index) const override;
};

class HeaderUnionInstance : public StructBase, public FunctionClass {
 private:
    z3::expr get_valid() const;

 public:
    explicit HeaderUnionInstance(P4State *state,
                                 const IR::Type_HeaderUnion *type, cstring name,
                                 uint64_t member_id);

    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "HeaderUnionInstance"; }
    cstring to_string() const override {
        cstring ret = "HeaderUnionInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    void update_validity(const HeaderInstance *child,
                         const z3::expr &valid_val);
    void isValid(Visitor *visitor, const IR::Vector<IR::Argument> *args);
    HeaderUnionInstance *copy() const override;
    // copy constructor
    HeaderUnionInstance(const HeaderUnionInstance &other);
    // overload = operator
    HeaderUnionInstance &operator=(const HeaderUnionInstance &other);
};

class EnumBase : public StructBase, public ValContainer {
    using StructBase::StructBase;

 protected:
    const IR::Type_Bits *member_type = &P4_STD_BIT_TYPE;

 public:
    EnumBase(P4State *state, const IR::Type *type, cstring name,
             uint64_t member_id);
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "EnumBase"; }
    cstring to_string() const override {
        cstring ret = "EnumBase(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    void set_undefined() override;
    void add_enum_member(cstring error_name);
    void bind(const z3::expr *bind_var = nullptr, uint64_t offset = 0) override;
    void merge(const z3::expr &cond, const P4Z3Instance &then_expr) override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
    P4Z3Instance *operator&(const P4Z3Instance &other) const override;
    P4Z3Instance *operator|(const P4Z3Instance &other) const override;
    void set_enum_val(const z3::expr &enum_input);
    EnumBase(const EnumBase &other);
    virtual EnumBase *instantiate(const NumericVal &enum_val) const = 0;

    // overload = operator
};

class EnumInstance : public EnumBase {
 public:
    EnumInstance(P4State *state, const IR::Type_Enum *type, cstring name,
                 uint64_t member_id);
    cstring get_static_type() const override { return "EnumInstance"; }
    cstring to_string() const override {
        cstring ret = "EnumInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    // copy constructor
    EnumInstance *copy() const override;
    EnumInstance(const EnumInstance &other);
    // overload = operator
    EnumInstance &operator=(const EnumInstance &other);
    EnumInstance *instantiate(const NumericVal &enum_val) const override;
};

class ErrorInstance : public EnumBase {
 public:
    ErrorInstance(P4State *state, const IR::Type_Error *type, cstring name,
                  uint64_t member_id);
    cstring get_static_type() const override { return "ErrorInstance"; }
    ErrorInstance *copy() const override;

    cstring to_string() const override {
        cstring ret = "ErrorInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    ErrorInstance *instantiate(const NumericVal &enum_val) const override;
};

class SerEnumInstance : public EnumBase {
 public:
    SerEnumInstance(P4State *state,
                    const ordered_map<cstring, P4Z3Instance *> &input_members,
                    const IR::Type_SerEnum *type, cstring name,
                    uint64_t member_id);
    cstring get_static_type() const override { return "SerEnumInstance"; }
    cstring to_string() const override {
        cstring ret = "SerSerEnumInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    // TODO: SerEnumInstance is static, so no copy allowed
    SerEnumInstance *copy() const override;
    SerEnumInstance *instantiate(const NumericVal &enum_val) const override;
    P4Z3Instance *operator&(const P4Z3Instance &other) const override;
    P4Z3Instance *operator|(const P4Z3Instance &other) const override;
};

class ListInstance : public StructBase {
 public:
    explicit ListInstance(P4State *state,
                          const std::vector<P4Z3Instance *> &val_list,
                          const IR::Type *type);
    explicit ListInstance(P4State *state, const IR::Type_List *list_type,
                          cstring name, uint64_t member_id);
    cstring get_static_type() const override { return "ListInstance"; }
    cstring to_string() const override {
        cstring ret = "ListInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first) {
                ret += ", ";
            }
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    ListInstance *copy() const override;
    std::vector<P4Z3Instance *> get_val_list() const;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class ControlInstance : public P4Z3Instance, public FunctionClass {
 private:
    P4State *state;
    VarMap resolved_const_args;
    std::map<cstring, const IR::Type *> local_type_map;
    // A wrapper class for table declarations
 public:
    // constructor
    explicit ControlInstance(P4State *state, const IR::Type *decl,
                             const VarMap &input_const_args);
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override{};
    ControlInstance *copy() const override {
        return new ControlInstance(state, p4_type, resolved_const_args);
    }

    void apply(Visitor *, const IR::Vector<IR::Argument> *);

    cstring get_static_type() const override { return "ControlInstance"; }
    cstring to_string() const override {
        cstring ret = "ControlInstance(";
        return ret + p4_type->toString() + ")";
    }
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
};

class P4Declaration : public P4Z3Instance {
    // A wrapper class for declarations
 private:
    const IR::StatOrDecl *decl;

 public:
    // constructor
    // TODO: This is a declaration, not an object. Distinguish!
    explicit P4Declaration(const IR::StatOrDecl *decl)
        : P4Z3Instance(nullptr), decl(decl) {}
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override{};
    // TODO: This is a little pointless....
    P4Declaration *copy() const override { return new P4Declaration(decl); }

    cstring get_static_type() const override { return "P4Declaration"; }
    cstring to_string() const override {
        cstring ret = "P4Declaration(";
        return ret + decl->toString() + ")";
    }
    const IR::StatOrDecl *get_decl() const { return decl; }
};

class P4TableInstance : public P4Declaration, public FunctionClass {
    // A wrapper class for table declarations
 private:
    P4State *state;
    ordered_map<cstring, P4Z3Instance *> members;

 public:
    z3::expr hit;
    TableProperties table_props;
    // constructor
    explicit P4TableInstance(P4State *state, const IR::P4Table *p4t);
    explicit P4TableInstance(P4State *state, const IR::StatOrDecl *decl,
                             z3::expr hit, TableProperties table_props);
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override {}

    P4TableInstance *copy() const override {
        return new P4TableInstance(state, get_decl(), hit, table_props);
    }

    P4Z3Instance *get_member(cstring name) const override {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    void apply(Visitor *, const IR::Vector<IR::Argument> *);

    cstring get_static_type() const override { return "P4TableInstance"; }
    cstring to_string() const override {
        cstring ret = "P4TableInstance(";
        return ret + get_decl()->toString() + ")";
    }
    z3::expr
    produce_const_match(Visitor *visitor,
                        std::vector<const P4Z3Instance *> *evaluated_keys,
                        const IR::ListExpression *entry_keys) const;
};

class ExternInstance : public P4Z3Instance, public FunctionClass {
 private:
    std::map<cstring, const IR::Method *> methods;
    P4State *state;
    const IR::Type_Extern *extern_type;

 public:
    explicit ExternInstance(P4State *state, const IR::Type_Extern *type);
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override{};
    cstring get_static_type() const override { return "ExternInstance"; }
    cstring to_string() const override {
        cstring ret = "ExternInstance(";
        ret += ")";
        return ret;
    }

    FunOrMethod get_function(cstring name) const override {
        if (methods.count(name) > 0) {
            return methods.at(name);
        }
        return FunctionClass::get_function(name);
    }
    // TODO: This is a little pointless....
    ExternInstance *copy() const override {
        return new ExternInstance(state, extern_type);
    }
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
};

}  // namespace TOZ3

#endif  // TOZ3_COMMON_TYPE_COMPLEX_H_
