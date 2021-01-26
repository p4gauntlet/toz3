#ifndef _TOZ3_COMPLEX_TYPE_H_
#define _TOZ3_COMPLEX_TYPE_H_

#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string

#include "ir/ir.h"
#include "state.h"

namespace TOZ3_V2 {

class P4ComplexInstance {
 public:
    P4ComplexInstance() {}
    template <typename T> bool is() const { return to<T>() != nullptr; }
    template <typename T> const T *to() const {
        return dynamic_cast<const T *>(this);
    }
    template <typename T> const T &as() const {
        return dynamic_cast<const T &>(*this);
    }
};

class StructInstance : public P4ComplexInstance {
 public:
    const IR::Type_StructLike *p4_type;
    std::map<cstring, boost::any> members;
    uint64_t member_id;
    uint64_t width;
    StructInstance(P4State *state, const IR::Type_StructLike *type,
                   uint64_t member_id);
    void bind(z3::ast bind_const);
};

class EnumInstance : public P4ComplexInstance {
 private:
 public:
    const IR::Type_Enum *p4_type;
    std::map<cstring, boost::any> members;
    uint64_t width;
    EnumInstance(P4State *state, const IR::Type_Enum *type);
};

class ErrorInstance : public P4ComplexInstance {
 private:
 public:
    const IR::Type_Error *p4_type;
    std::map<cstring, boost::any> members;
    uint64_t width;
    ErrorInstance(P4State *state, const IR::Type_Error *type);
};

} // namespace TOZ3_V2

#endif // _TOZ3_COMPLEX_TYPE_H_
