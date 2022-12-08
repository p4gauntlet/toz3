#ifndef TOZ3_COMMON_VISITOR_SPECIALIZE_H_
#define TOZ3_COMMON_VISITOR_SPECIALIZE_H_

#include <map>

#include "ir/ir.h"
#include "ir/node.h"
#include "ir/vector.h"
#include "ir/visitor.h"
#include "lib/cstring.h"
#include "lib/exceptions.h"
#include "state.h"

namespace TOZ3 {

class TypeModifier : public Transform {
 private:
    const std::map<cstring, const IR::Type*>* type_mapping;
    const IR::Node* postorder(IR::Type* type) override;

 public:
    explicit TypeModifier(const std::map<cstring, const IR::Type*>* type_mapping)
        : type_mapping(type_mapping) {
        visitDagOnce = false;
    }
};

class TypeSpecializer : public Transform {
 private:
    const P4State& state;
    const IR::Vector<IR::Type>& types;

    const IR::Node* preorder(IR::Node* node) override {
        P4C_UNIMPLEMENTED("TypeSpecializer: IR Node %s not implemented!", node->node_type_name());
    }
    const IR::Node* preorder(IR::Type_Extern* te) override;
    const IR::Node* preorder(IR::P4Control* c) override;
    const IR::Node* preorder(IR::Type_Control* tc) override;
    const IR::Node* preorder(IR::P4Parser* p) override;
    const IR::Node* preorder(IR::Type_Parser* tp) override;
    const IR::Node* preorder(IR::Type_Package* tp) override;
    const IR::Node* preorder(IR::Type_Var* tv) override;
    const IR::Node* preorder(IR::Type_Name* tn) override;
    const IR::Node* preorder(IR::Type_StructLike* ts) override;
    const IR::Node* preorder(IR::P4Action* a) override;
    const IR::Node* preorder(IR::Function* f) override;
    const IR::Node* preorder(IR::Method* m) override;

 public:
    explicit TypeSpecializer(const P4State& state, const IR::Vector<IR::Type>& types)
        : state(state), types(types) {
        visitDagOnce = false;
    }
};

}  // namespace TOZ3

#endif  // TOZ3_COMMON_VISITOR_SPECIALIZE_H_
