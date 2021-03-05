#ifndef _TOZ3_COMPLEX_TYPE_H_
#define _TOZ3_COMPLEX_TYPE_H_

#include <cstdio>
#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string
#include <vector> // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"

#include "type_simple.h"

namespace TOZ3_V2 {

class StructBase : public P4Z3Instance {
 protected:
    P4State *state;
    std::map<cstring, P4Z3Instance *> members;
    std::map<cstring, std::function<void()>> member_functions;
    std::map<cstring, const IR::Type *> member_types;
    uint64_t member_id;
    uint64_t width;

 public:
    const IR::Type_StructLike *p4_type;
    StructBase(P4State *state, const IR::Type_StructLike *type,
               uint64_t member_id);
    StructBase() {}
    virtual std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "") const;

    uint64_t get_width() { return width; }

    const P4Z3Instance *get_const_member(cstring name) const {
        return members.at(name);
    }
    P4Z3Instance *get_member(cstring name) { return members.at(name); }
    const IR::Type *get_member_type(cstring name) {
        return member_types.at(name);
    }

    std::function<void()> get_function(cstring name) {
        return member_functions.at(name);
    }

    void update_member(cstring name, P4Z3Instance *val) {
        members.at(name) = val;
    }
    void insert_member(cstring name, P4Z3Instance *val) {
        members.insert({name, val});
    }
    std::map<cstring, P4Z3Instance *> *get_member_map() { return &members; }
    const std::map<cstring, P4Z3Instance *> *get_immutable_member_map() const {
        return &members;
    }
    virtual void propagate_validity(z3::expr * = nullptr) {}

    ~StructBase() {}
    // copy constructor
    StructBase(const StructBase &other);
    // overload = operator
    StructBase &operator=(const StructBase &other);

    void merge(z3::expr *cond, const P4Z3Instance *) override;
};

class StructInstance : public StructBase {
    using StructBase::StructBase;

 public:
    StructInstance *copy() const override;
    void propagate_validity(z3::expr *valid_expr = nullptr) override;
    cstring get_static_type() const override { return "StructInstance"; }
    cstring get_static_type() override { return "StructInstance"; }
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
};

class HeaderInstance : public StructBase {
    using StructBase::StructBase;

 private:
    z3::expr valid;

 public:
    HeaderInstance(P4State *state, const IR::Type_StructLike *type,
                   uint64_t member_id);

    void set_valid(z3::expr *valid_val);

    const z3::expr *get_valid() const;

    void setValid();
    void setInvalid();
    void isValid();
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "") const override;
    void propagate_validity(z3::expr *valid_expr = nullptr) override;
    void merge(z3::expr *cond, const P4Z3Instance *) override;
    HeaderInstance *copy() const override;
    cstring get_static_type() const override { return "HeaderInstance"; }
    cstring get_static_type() override { return "HeaderInstance"; }
    cstring to_string() const override {
        cstring ret = "HeaderInstance(";
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
};

class EnumInstance : public StructBase {
    using StructBase::StructBase;

 private:
    P4State *state;
    uint64_t member_id;
    uint64_t width;

 public:
    const IR::Type_Enum *p4_type;
    EnumInstance(P4State *state, const IR::Type_Enum *type, uint64_t member_id);
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "") const override;
    cstring get_static_type() const override { return "EnumInstance"; }
    cstring get_static_type() override { return "EnumInstance"; }
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
};

class ErrorInstance : public StructBase {
    using StructBase::StructBase;

 private:
    P4State *state;
    uint64_t member_id;
    uint64_t width;

 public:
    const IR::Type_Error *p4_type;
    ErrorInstance(P4State *state, const IR::Type_Error *type,
                  uint64_t member_id);
    std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring prefix = "") const override;
    cstring get_static_type() const override { return "ErrorInstance"; }
    cstring get_static_type() override { return "ErrorInstance"; }
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

class ExternInstance : public P4Z3Instance {
 private:
    std::map<cstring, const IR::Method *> methods;

 public:
    const IR::Type_Extern *p4_type;
    ExternInstance(P4State *state, const IR::Type_Extern *type);
    void merge(z3::expr *, const P4Z3Instance *) override{
        // Merge is a no-op here.
    };
    cstring get_static_type() const override { return "ExternInstance"; }
    cstring get_static_type() override { return "ExternInstance"; }
    cstring to_string() const override {
        cstring ret = "ExternInstance(";
        ret += ")";
        return ret;
    }
    const IR::Method *get_method(cstring method_name) {
        if (methods.count(method_name)) {
            return methods.at(method_name);
        }
        error("Extern %s has no method %s.", p4_type, method_name);
        exit(1);
    }
};

} // namespace TOZ3_V2

#endif // _TOZ3_COMPLEX_TYPE_H_
