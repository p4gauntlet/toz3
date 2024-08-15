#include "counter.h"
namespace P4::ToZ3::Pruner {
Visitor::profile_t Counter::init_apply(const IR::Node *node) { return Inspector::init_apply(node); }

bool Counter::preorder(const IR::Statement * /*s*/) {
    statements++;
    return true;
}
}  // namespace P4::ToZ3::Pruner
