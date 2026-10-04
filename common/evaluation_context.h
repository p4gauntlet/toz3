#ifndef TOZ3_COMMON_EVALUATION_CONTEXT_H_
#define TOZ3_COMMON_EVALUATION_CONTEXT_H_

#include <z3++.h>

#include "ir/node.h"
#include "lib/cstring.h"

namespace P4::ToZ3 {

// Runtime values need evaluation and parser control, independently of the AST visitor used
// to implement them. Evaluation preserves the caller's current scope and traversal context.
class EvaluationContext {
 public:
    virtual ~EvaluationContext() = default;
    virtual void evaluate(const IR::Node *node) = 0;
};

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_EVALUATION_CONTEXT_H_
