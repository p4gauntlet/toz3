#ifndef _PRUNE_UNUSED_H_
#define _PRUNE_UNUSED_H_

#include <vector>

#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/unusedDeclarations.h"
#include "ir/ir.h"
#include "ir/node.h"
#include "ir/pass_manager.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/null.h"
#include "lib/safe_vector.h"

namespace P4::ToZ3::Pruner {

struct struct_obj {
    cstring name;
    std::vector<cstring> *fields{};
};

class PruneUnused : public P4::UnusedDeclarations {
    const std::vector<struct_obj *> *_used_structs;

 public:
    explicit PruneUnused(const UsedDeclSet &used, std::vector<struct_obj *> *used_structs)
        : UnusedDeclarations(used, true, true, true), _used_structs(used_structs) {
        setName("PruneUnused");
    }
    const IR::Node *preorder(IR::Type_StructLike *ts) override;
    const IR::Node *preorder(IR::StructField *sf) override;
    const IR::Node *preorder(IR::Type_Extern *te) override;
    const IR::Node *preorder(IR::Method *m) override;
    const IR::Node *preorder(IR::Function *f) override;
    void show_used_structs();
    bool check_if_field_used(cstring name_of_struct, cstring name_of_field);
};

class ListStructs : public Inspector, ResolutionContext {
    std::vector<struct_obj *> *_used_structs;

 public:
    explicit ListStructs(std::vector<struct_obj *> *used_structs) : _used_structs(used_structs) {
        CHECK_NULL(_used_structs);
        setName("ListStructs");
    }
    Visitor::profile_t init_apply(const IR::Node *node) override;
    bool preorder(const IR::Member *p) override;
    void insertField(cstring name_of_struct, cstring name_of_field);
};

class ExtendedUnusedDeclarations : public PassManager {
 public:
    explicit ExtendedUnusedDeclarations(std::vector<struct_obj *> *used_structs) {
        const UsedDeclSet *used_decls = new UsedDeclSet();
        passes.emplace_back(new PassRepeated{
            new ListStructs(used_structs),
            new PruneUnused(*used_decls, used_structs),
        });
        setName("ExtendedUnusedDeclarations");
    }
};

}  // namespace P4::ToZ3::Pruner

#endif /* _PRUNE_UNUSED_H_ */
