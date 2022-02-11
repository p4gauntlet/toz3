#ifndef _PRUNE_UNUSED_H_
#define _PRUNE_UNUSED_H_

#include <vector>
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/unusedDeclarations.h"
#include "ir/ir.h"

namespace P4PRUNER {

struct struct_obj {
    cstring name;
    std::vector<cstring> *fields;
};

class PruneUnused : public P4::RemoveUnusedDeclarations {
    // the refmap of the parent class is private so we have to use our own
    // a little bit unfortunate but the pointer is the same
    // we also do not modify the map so this is fairly safe
    const P4::ReferenceMap *unused_refMap;
    const std::vector<struct_obj *> *used_structs;

 public:
    explicit PruneUnused(const P4::ReferenceMap *refMap,
                         std::vector<struct_obj *> *_used_structs)
        : P4::RemoveUnusedDeclarations(refMap), unused_refMap(refMap),
          used_structs(_used_structs) {
        CHECK_NULL(refMap);
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

class ListStructs : public Inspector {
    const P4::ReferenceMap *unused_refMap;
    std::vector<struct_obj *> *used_structs;

 public:
    explicit ListStructs(const P4::ReferenceMap *refMap,
                         std::vector<struct_obj *> *_used_structs)
        : unused_refMap(refMap), used_structs(_used_structs) {
        CHECK_NULL(refMap);
        setName("ListStructs");
    }
    Visitor::profile_t init_apply(const IR::Node *node) override;
    bool preorder(const IR::Member *p) override;
    void insertField(cstring name_of_struct, cstring name_of_field);
};

class ExtendedUnusedDeclarations : public PassManager {
 public:
    explicit ExtendedUnusedDeclarations(
        P4::ReferenceMap *refMap, std::vector<struct_obj *> *used_structs) {
        CHECK_NULL(refMap);
        passes.emplace_back(
            new PassRepeated{new P4::ResolveReferences(refMap),
                             new ListStructs(refMap, used_structs),
                             new PruneUnused(refMap, used_structs)});
        setName("ExtendedUnusedDeclarations");
    }
};

}  // namespace P4PRUNER

#endif /* _PRUNE_UNUSED_H_ */
