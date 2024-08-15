#ifndef _REPLACE_VARIABLES_H
#define _REPLACE_VARIABLES_H
// #include <vector>

#include "frontends/p4/typeMap.h"
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "lib/null.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

class ReplaceVariables : public Transform {
    P4::TypeMap *typeMap;

 public:
    explicit ReplaceVariables(P4::TypeMap *typeMap) : typeMap(typeMap) {
        visitDagOnce = true;
        CHECK_NULL(typeMap);
        setName("ReplaceVariables");
    }

    const IR::Node *postorder(IR::Expression *s);
    const IR::Node *postorder(IR::MethodCallExpression *s);
};

const IR::P4Program *replace_variables(const IR::P4Program *program, PrunerConfig pruner_conf);

}  // namespace P4::ToZ3::Pruner

#endif /* _REPLACE_VARIABLES_H */
