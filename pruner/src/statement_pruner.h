
#ifndef _STATEMENT_PRUNER_H
#define _STATEMENT_PRUNER_H
#include <cstdint>
#include <vector>

#include "ir/ir.h"
#include "ir/node.h"
#include "ir/visitor.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

class Pruner : public Transform {
 public:
    const std::vector<const IR::Statement *> &to_prune;
    explicit Pruner(const std::vector<const IR::Statement *> &to_prune);
    const IR::Node *preorder(IR::Statement *s) override;
    const IR::Node *preorder(IR::ReturnStatement *s) override;
    const IR::Node *preorder(IR::BlockStatement *s) override;
    const IR::Node *preorder(IR::EmptyStatement *e) override;
};

class Collector : public Inspector {
 public:
    explicit Collector(uint64_t _max_statements) {
        setName("Collector");
        max_statements = _max_statements;
    }
    Visitor::profile_t init_apply(const IR::Node *node) override;
    bool preorder(const IR::Statement *s) override;
    bool preorder(const IR::BlockStatement *s) override;
    std::vector<const IR::Statement *> to_prune;
    uint64_t max_statements;
};

const IR::P4Program *prune_statements(const IR::P4Program *program, PrunerConfig pruner_conf,
                                      uint64_t prog_size);

}  // namespace P4::ToZ3::Pruner

#endif /* _STATEMENT_PRUNER_H */
