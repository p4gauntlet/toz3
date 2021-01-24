#ifndef _TOZ3_COMPLEX_TYPE_H_
#define _TOZ3_COMPLEX_TYPE_H_

#include <z3++.h>

#include <map>    // std::map
#include <string> // std::to_string

#include "ir/ir.h"
#include "state.h"

namespace TOZ3_V2 {

template <typename Base, typename T> inline bool instanceof (const T *) {
    return std::is_base_of<Base, T>::value;
}

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
                   uint64_t member_id)
        : p4_type(type), member_id(member_id) {
        width = 0;
        uint64_t flat_id = member_id;

        for (auto field : type->fields) {
            cstring name = cstring(std::to_string(flat_id));
            auto member = state->gen_instance(name, field->type);
            members.emplace(name, member);
            if (field->type->is<IR::Type_Name>()) {
                auto si = boost::any_cast<StructInstance>(member);
                width += si.width;
            } else if (auto tbi = field->type->to<IR::Type_Bits>()) {
                width += tbi->width_bits();
            } else if (auto tvb = field->type->to<IR::Type_Varbits>()) {
                width += tvb->width_bits();
            } else if (field->type->is<IR::Type_Boolean>()) {
                width++;
            } else {
                BUG("Type \"%s\" not supported!.", field->type);
            }
            flat_id++;
        }
    }

    void bind(z3::ast bind_const);
};
} // namespace TOZ3_V2

#endif // _TOZ3_COMPLEX_TYPE_H_
