#ifndef TOZ3_COMMON_TYPE_INFERENCE_H_
#define TOZ3_COMMON_TYPE_INFERENCE_H_

#include <map>
#include <vector>

#include "ir/ir.h"

namespace P4::ToZ3 {
class P4State;

using TypeBindings = std::map<cstring, const IR::Type *>;

struct TypeConstraint {
    const IR::Type *formal;
    const IR::Type *actual;
};

// Call-local inference: decompose matching type constructors into X = type
// constraints, then solve only the variables declared by this call. This pass
// neither evaluates expressions nor changes the interpreter's type environment.
TypeBindings infer_type_arguments(const P4State &state, const IR::TypeParameters &parameters,
                                  const std::vector<TypeConstraint> &constraints);
bool contains_type_variables(const IR::Type *type, const IR::TypeParameters &parameters);
const IR::Type *architecture_type(const IR::Type *type);

// Determine a receiver's type without evaluating it or its indexing expressions.
const IR::Type *expression_type(const P4State &state, const IR::Expression *expression);
big_int serialized_size(const P4State &state, const IR::Type *type, bool maximum);
bool arguments_match(const IR::ParameterList &parameters, const IR::Vector<IR::Argument> &arguments,
                     bool action = false);
}  // namespace P4::ToZ3
#endif  // TOZ3_COMMON_TYPE_INFERENCE_H_
