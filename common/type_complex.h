#ifndef _TOZ3_COMPLEX_TYPE_H_
#define _TOZ3_COMPLEX_TYPE_H_

#include <cstdio>

#include <map>     // std::map
#include <string>  // std::to_string
#include <utility> // std::pair
#include <vector>  // std::vector

#include "z3++.h"
#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_simple.h"

namespace TOZ3_V2 {

class StructBase : public P4Z3Instance {
 protected:
    P4State *state;
    ordered_map<cstring, P4Z3Instance *> members;
    std::map<cstring, const IR::Type *> member_types;
    uint64_t width;
    z3::expr valid;
    cstring instance_name;

 public:
    const IR::Type *p4_type;
    StructBase(P4State *state, const IR::Type *type, uint64_t member_id,
               cstring prefix);

    uint64_t get_width() { return width; }

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
    const IR::Type *get_member_type(cstring name) const {
        auto it = member_types.find(name);
        if (it != member_types.end()) {
            return it->second;
        }
        BUG("Name %s not found in member type map of %s.", name,
            get_static_type());
    }

    void update_member(cstring name, P4Z3Instance *val) {
        members.at(name) = val;
    }
    void insert_member(cstring name, P4Z3Instance *val) {
        members.insert({name, val});
    }
    const ordered_map<cstring, P4Z3Instance *> *get_member_map() const {
        return &members;
    }
    const ordered_map<cstring, P4Z3Instance *> *
    get_immutable_member_map() const {
        return &members;
    }
    void set_undefined() override;
    virtual void propagate_validity(const z3::expr *valid_expr = nullptr);
    virtual void bind(uint64_t member_id = 0, cstring prefix = "");
    virtual void set_list(std::vector<P4Z3Instance *>);

    ~StructBase() {}
    // copy constructor
    StructBase(const StructBase &other);
    // overload = operator
    StructBase &operator=(const StructBase &other);
    void merge(const z3::expr &cond, const P4Z3Instance &) override;
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class StructInstance : public StructBase {
    using StructBase::StructBase;

 public:
    StructInstance(P4State *state, const IR::Type_StructLike *type,
                   uint64_t member_id, cstring prefix);
    StructInstance *copy() const override;
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "StructInstance"; }
    cstring to_string() const override {
        cstring ret = "StructInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    ~StructInstance() {}
    // copy constructor
    StructInstance(const StructInstance &other);
    // overload = operator
    StructInstance &operator=(const StructInstance &other);
};

class HeaderInstance : public StructInstance {
    using StructInstance::StructInstance;

 private:
    std::map<cstring, FunctionWrapper *> member_functions;

 public:
    HeaderInstance(P4State *state, const IR::Type_Header *type,
                   uint64_t member_id, cstring prefix);
    void set_valid(const z3::expr &valid_val);
    const z3::expr *get_valid() const;
    void setValid(Visitor *, const IR::Vector<IR::Argument> *);
    void setInvalid(Visitor *, const IR::Vector<IR::Argument> *);
    void isValid(Visitor *, const IR::Vector<IR::Argument> *);
    void propagate_validity(const z3::expr *valid_expr = nullptr) override;
    void merge(const z3::expr &cond, const P4Z3Instance &) override;
    void set_list(std::vector<P4Z3Instance *>) override;

    P4Z3Instance *get_function(cstring name) const override {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }

    cstring get_static_type() const override { return "HeaderInstance"; }
    cstring to_string() const override {
        cstring ret = "HeaderInstance(";
        ret += "valid: " + valid.to_string() + ", ";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    ~HeaderInstance() {}
    // copy constructor
    HeaderInstance *copy() const override;
    HeaderInstance(const HeaderInstance &other);
    // overload = operator
    HeaderInstance &operator=(const HeaderInstance &other);
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class StackInstance : public StructBase {
 private:
    std::map<cstring, FunctionWrapper *> member_functions;
    mutable Z3Int nextIndex;
    mutable Z3Int lastIndex;
    mutable Z3Int size;
    size_t int_size;
    const IR::Type *elem_type;

 public:
    explicit StackInstance(P4State *state, const IR::Type_Stack *type,
                           uint64_t member_id, cstring prefix);

    P4Z3Instance *get_function(cstring name) const override {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }

    P4Z3Instance *get_member(const P4Z3Instance *index) const;
    P4Z3Instance *get_member(cstring name) const override;
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "StackInstance"; }
    cstring to_string() const override {
        cstring ret = "StackInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    size_t get_int_size() const { return int_size; }
    void push_front(Visitor *, const IR::Vector<IR::Argument> *);
    void pop_front(Visitor *, const IR::Vector<IR::Argument> *);

    // copy constructor
    StackInstance *copy() const override;
    StackInstance(const StackInstance &other);
    // overload = operator
    StackInstance &operator=(const StackInstance &other);
    ~StackInstance() {}
};

class EnumBase : public StructBase {
    using StructBase::StructBase;

 public:
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "",
                const z3::expr *valid_expr = nullptr) const override;
    cstring get_static_type() const override { return "EnumBase"; }
    cstring to_string() const override {
        cstring ret = "EnumBase(";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }

    void add_enum_member(cstring error_name);
    z3::expr operator==(const P4Z3Instance &other) const override;
    z3::expr operator!=(const P4Z3Instance &other) const override;
};

class EnumInstance : public EnumBase {
 public:
    EnumInstance(P4State *state, const IR::Type_Enum *type, uint64_t member_id,
                 cstring prefix);
    cstring get_static_type() const override { return "EnumInstance"; }
    cstring to_string() const override {
        cstring ret = "EnumInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
    // TODO: EnumInstance is static, so no copy allowed
    EnumInstance *copy() const override;
};

class ErrorInstance : public EnumBase {

 public:
    ErrorInstance(P4State *state, const IR::Type_Error *type,
                  uint64_t member_id, cstring prefix);
    cstring get_static_type() const override { return "ErrorInstance"; }
    ErrorInstance *copy() const override;

    cstring to_string() const override {
        cstring ret = "ErrorInstance(";
        bool first = true;
        for (auto tuple : members) {
            if (!first)
                ret += ", ";
            ret += tuple.first + ": " + tuple.second->to_string();
            first = false;
        }
        ret += ")";
        return ret;
    }
}; // namespace TOZ3_V2

class ListInstance : public P4Z3Instance {
 private:
    P4State *state;
    std::vector<P4Z3Instance *> val_list;

 public:
    const IR::Type *p4_type;
    ListInstance(P4State *state, std::vector<P4Z3Instance *> val_list,
                 const IR::Type *type)
        : state(state), val_list(val_list), p4_type(type) {}

    cstring get_static_type() const override { return "ListInstance"; }
    cstring to_string() const override {
        cstring ret = "ListInstance(";
        ret += ")";
        return ret;
    }
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
    std::vector<P4Z3Instance *> get_val_list() const { return val_list; }
    ListInstance *copy() const override;
};

class P4Declaration : public P4Z3Instance {
    // A wrapper class for declarations
 protected:
    ordered_map<cstring, P4Z3Instance *> members;

 public:
    const IR::Declaration *decl;
    // constructor
    explicit P4Declaration(const IR::Declaration *decl) : decl(decl) {}
    // Merge is a no-op here.
    void merge(const z3::expr &, const P4Z3Instance &) override {}
    // TODO: This is a little pointless....
    P4Declaration *copy() const override { return new P4Declaration(decl); }

    cstring get_static_type() const override { return "P4Declaration"; }
    cstring to_string() const override {
        cstring ret = "P4Declaration(";
        return ret + decl->toString() + ")";
    }
};

class P4TableInstance : public P4Declaration {
 private:
    P4State *state;
    std::map<cstring, FunctionWrapper *> member_functions;
    // A wrapper class for table declarations
 public:
    cstring table_name;
    const z3::expr hit;
    std::vector<const IR::KeyElement *> keys;
    std::vector<const IR::MethodCallExpression *> actions;
    bool immutable;
    // constructor
    explicit P4TableInstance(P4State *state, const IR::Declaration *decl);
    explicit P4TableInstance(
        P4State *state, const IR::Declaration *decl, cstring table_name,
        const z3::expr hit, std::vector<const IR::KeyElement *> keys,
        std::vector<const IR::MethodCallExpression *> actions, bool immutable);
    // Merge is a no-op here.
    void merge(const z3::expr &, const P4Z3Instance &) override {}
    // TODO: This is a little pointless....
    P4TableInstance *copy() const override {
        return new P4TableInstance(state, decl, table_name, hit, keys, actions,
                                   immutable);
    }

    P4Z3Instance *get_member(cstring name) const override {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    P4Z3Instance *get_function(cstring name) const override {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }
    void apply(Visitor *, const IR::Vector<IR::Argument> *);

    cstring get_static_type() const override { return "P4TableInstance"; }
    cstring to_string() const override {
        cstring ret = "P4TableInstance(";
        return ret + decl->toString() + ")";
    }
};

class DeclarationInstance : public P4Z3Instance {
 private:
    P4State *state;
    std::map<cstring, FunctionWrapper *> member_functions;
    ordered_map<cstring, P4Z3Instance *> members;
    // A wrapper class for table declarations
 public:
    const IR::Type_Declaration *decl;
    // constructor
    explicit DeclarationInstance(P4State *state,
                                 const IR::Type_Declaration *decl);
    // Merge is a no-op here.
    void merge(const z3::expr &, const P4Z3Instance &) override {}
    // TODO: This is a little pointless....
    DeclarationInstance *copy() const override {
        return new DeclarationInstance(state, decl);
    }

    P4Z3Instance *get_member(cstring name) const override {
        auto it = members.find(name);
        if (it != members.end()) {
            return it->second;
        }
        BUG("Name %s not found in member map.", name);
    }
    P4Z3Instance *get_function(cstring name) const override {
        auto it = member_functions.find(name);
        if (it != member_functions.end()) {
            return it->second;
        }
        BUG("Name %s not found in function map.", name);
    }
    void apply(Visitor *, const IR::Vector<IR::Argument> *);

    cstring get_static_type() const override { return "DeclarationInstance"; }
    cstring to_string() const override {
        cstring ret = "DeclarationInstance(";
        return ret + decl->toString() + ")";
    }
};

class ExternInstance : public P4Z3Instance {
 private:
    std::map<cstring, P4Declaration *> methods;
    P4State *state;
    const IR::Type_Extern *p4_type;

 public:
    explicit ExternInstance(P4State *state, const IR::Type_Extern *type);
    void merge(const z3::expr &, const P4Z3Instance &) override{
        // Merge is a no-op here.
    };
    cstring get_static_type() const override { return "ExternInstance"; }
    cstring to_string() const override {
        cstring ret = "ExternInstance(";
        ret += ")";
        return ret;
    }
    P4Z3Instance *get_function(cstring method_name) const override {
        if (methods.count(method_name)) {
            return methods.at(method_name);
        }
        error("Extern %s has no method %s.", p4_type, method_name);
        exit(1);
    }
    // TODO: This is a little pointless....
    ExternInstance *copy() const override {
        return new ExternInstance(state, p4_type);
    }
};

} // namespace TOZ3_V2

#endif // _TOZ3_COMPLEX_TYPE_H_
