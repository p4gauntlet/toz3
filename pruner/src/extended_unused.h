#ifndef _PRUNE_UNUSED_H_
#define _PRUNE_UNUSED_H_

#include <map>
#include <set>

#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/p4/unusedDeclarations.h"
#include "ir/ir.h"
#include "ir/pass_manager.h"
#include "lib/cstring.h"

namespace P4::ToZ3::Pruner {

// Names also cover specialized instances of a generic struct. Sharing a name
// between scopes can only retain extra fields, never remove a referenced field.
using UsedFields = std::map<cstring, std::set<cstring>>;

class PruneUnused : public P4::UnusedDeclarations {
    const UsedFields &usedFields;

 public:
    PruneUnused(const UsedDeclSet &used, const UsedFields &usedFields)
        : UnusedDeclarations(used, true, false, false), usedFields(usedFields) {
        setName("PruneUnused");
    }
    const IR::Node *preorder(IR::Type_StructLike *type) override;
    const IR::Node *preorder(IR::StructField *field) override;
    const IR::Node *preorder(IR::Type_Extern *type) override;
};

class ListStructs : public Inspector {
    const P4::TypeMap &typeMap;
    UsedFields &usedFields;

    void retainFields(const IR::Type *type);

 public:
    ListStructs(const P4::TypeMap &typeMap, UsedFields &usedFields)
        : typeMap(typeMap), usedFields(usedFields) {
        setName("ListStructs");
        visitDagOnce = false;
    }
    Visitor::profile_t init_apply(const IR::Node *node) override;
    bool preorder(const IR::Expression *expression) override;
};

class ExtendedUnusedDeclarations : public PassManager {
    P4::TypeMap typeMap;
    UsedDeclSet usedDeclarations;
    UsedFields usedFields;

 public:
    ExtendedUnusedDeclarations() {
        addPasses({new PassRepeated({
            new P4::TypeInference(&typeMap, false),
            new P4::CollectUsedDeclarations(usedDeclarations),
            new ListStructs(typeMap, usedFields),
            new PruneUnused(usedDeclarations, usedFields),
        })});
        setStopOnError(true);
        setName("ExtendedUnusedDeclarations");
    }
};

}  // namespace P4::ToZ3::Pruner

#endif /* _PRUNE_UNUSED_H_ */
