#ifndef _PRUNER_SRC_COUNTER_H
#define _PRUNER_SRC_COUNTER_H

#include <cstdint>

#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"

namespace P4::ToZ3::Pruner {
class Counter : public Inspector {
 public:
    Visitor::profile_t init_apply(const IR::Node *node) override;
    bool preorder(const IR::Statement *s) override;
    uint64_t statements;

    Counter() { statements = 0; }
};
}  // namespace P4::ToZ3::Pruner
#endif /* _PRUNER_SRC_COUNTER_H */
