#ifndef _TOZ3_COMPLEX_TYPE_H_
#define _TOZ3_COMPLEX_TYPE_H_

#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string
#include <vector> // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"
#include "state.h"
#include "z3_int.h"

namespace TOZ3_V2 {

class StructInstance : public P4ComplexInstance {
 private:
    P4State *state;

 public:
    const IR::Type_StructLike *p4_type;
    std::map<cstring, P4Z3Instance> members;
    std::map<cstring, const IR::Type *> member_types;
    uint64_t member_id;
    uint64_t width;
    StructInstance(P4State *state, const IR::Type_StructLike *type,
                   uint64_t member_id);
    void bind(z3::expr bind_const);
    void merge(z3::expr cond, StructInstance *else_instance);
    std::vector<std::pair<cstring, z3::expr>> get_z3_vars(cstring prefix="");

    // destructor
    ~StructInstance() {}
    // copy constructor
    StructInstance(const StructInstance &other) : P4ComplexInstance(other) {
        for (auto value_tuple : other.members) {
            cstring name = value_tuple.first;
            P4Z3Instance var = value_tuple.second;
            if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
                members.insert({name, *z3_var});
            } else if (auto z3_var = check_complex<StructInstance>(var)) {
                StructInstance member_cpy = *z3_var;
                members.insert({name, &member_cpy});
            } else if (auto z3_var = check_complex<Z3Int>(var)) {
                Z3Int member_cpy = *z3_var;
                members.insert({name, &member_cpy});
            } else {
                BUG("Var is neither type z3::expr nor StructInstance!");
            }
        }
    }

    // overload = operator
    StructInstance &operator=(const StructInstance &other) {
        if (this == &other)
            return *this; // self assignment

        for (auto value_tuple : other.members) {
            cstring name = value_tuple.first;
            P4Z3Instance var = value_tuple.second;
            if (z3::expr *z3_var = boost::get<z3::expr>(&var)) {
                members.insert({name, *z3_var});
            } else if (auto z3_var = check_complex<StructInstance>(var)) {
                StructInstance member_cpy = *z3_var;
                members.insert({name, &member_cpy});
            } else if (auto z3_var = check_complex<Z3Int>(var)) {
                Z3Int member_cpy = *z3_var;
                members.insert({name, &member_cpy});
            } else {
                BUG("Var is neither type z3::expr nor StructInstance!");
            }
        }
        return *this;
    }
};

class EnumInstance : public P4ComplexInstance {
 private:
    P4State *state;

 public:
    const IR::Type_Enum *p4_type;
    std::map<cstring, P4Z3Instance> members;
    uint64_t width;
    uint64_t member_id;
    EnumInstance(P4State *state, const IR::Type_Enum *type, uint64_t member_id);
    std::vector<std::pair<cstring, z3::expr>> get_z3_vars(cstring prefix="");
};

class ErrorInstance : public P4ComplexInstance {
 private:
    P4State *state;

 public:
    const IR::Type_Error *p4_type;
    std::map<cstring, P4Z3Instance> members;
    uint64_t member_id;
    uint64_t width;
    ErrorInstance(P4State *state, const IR::Type_Error *type,
                  uint64_t member_id);
    std::vector<std::pair<cstring, z3::expr>> get_z3_vars(cstring prefix="");
};

class ExternInstance : public P4ComplexInstance {
 private:
 public:
    const IR::Type_Extern *p4_type;
    std::map<cstring, P4Z3Instance> members;
    uint64_t width;
    ExternInstance(P4State *state, const IR::Type_Extern *type);
};

} // namespace TOZ3_V2

#endif // _TOZ3_COMPLEX_TYPE_H_
