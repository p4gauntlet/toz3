#ifndef TOZ3_COMMON_DEFAULTS_H_
#define TOZ3_COMMON_DEFAULTS_H_

#include "type_base.h"

namespace P4::ToZ3 {
class P4State;

// A context-dependent `...` expression acquires its type when it is assigned.
class DefaultInstance : public P4Z3Instance {
    P4State *state;

 public:
    explicit DefaultInstance(P4State *state)
        : P4Z3Instance(IR::Type_Dontcare::get()), state(state) {}
    P4Z3Instance *cast_allocate(const IR::Type *type) const override;
    DefaultInstance *copy() const override { return new DefaultInstance(state); }
    cstring get_static_type() const override { return "DefaultInstance"_cs; }
    cstring to_string() const override { return "..."_cs; }
};
}  // namespace P4::ToZ3
#endif  // TOZ3_COMMON_DEFAULTS_H_
