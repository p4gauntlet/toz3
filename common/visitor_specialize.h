#ifndef _TOZ3_VISITOR_SPECIALIZE_H_
#define _TOZ3_VISITOR_SPECIALIZE_H_

#include <map>
#include <vector>

#include "../contrib/z3/z3++.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "ir/ir.h"
#include "visitor_interpret.h"

namespace TOZ3_V2 {

class DoTypeSpecialization : public Transform {
 public:
    P4State *state;
    P4::ReferenceMap &ref_map;
    explicit DoTypeSpecialization(P4State *state, P4::ReferenceMap &ref_map)
        : state(state), ref_map(ref_map) {
        visitDagOnce = false;
    }

    const IR::Node *postorder(IR::MethodCallExpression *) override;
};

class TypeSpecialization : public PassManager {
 public:
    explicit TypeSpecialization(P4State *state) {
        P4::ReferenceMap ref_map;
        passes.push_back(new P4::ResolveReferences(&ref_map, true));
        passes.push_back(new DoTypeSpecialization(state, ref_map));
    }
};

}  // namespace TOZ3_V2

#endif  // _TOZ3_VISITOR_SPECIALIZE_H_
