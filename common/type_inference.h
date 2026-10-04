#ifndef TOZ3_COMMON_TYPE_INFERENCE_H_
#define TOZ3_COMMON_TYPE_INFERENCE_H_

#include "ir/ir.h"

namespace P4::ToZ3 {
class P4State;

// Determine a receiver's type without evaluating it or its indexing expressions.
const IR::Type *expression_type(const P4State &state, const IR::Expression *expression);
big_int serialized_size(const P4State &state, const IR::Type *type, bool maximum);
}  // namespace P4::ToZ3
#endif  // TOZ3_COMMON_TYPE_INFERENCE_H_
