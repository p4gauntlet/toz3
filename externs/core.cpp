#include "toz3/common/type_complex.h"

namespace TOZ3 {

class PacketIn : public ExternInstance {
 private:
    std::map<cstring, const IR::Method *> methods;
    P4State *state;
    const IR::Type_Extern *extern_type;

 public:
    explicit PacketIn(P4State *state, const IR::Type_Extern *type);
    // Merge is a no-op here.
    void merge(const z3::expr & /*cond*/,
               const P4Z3Instance & /*then_expr*/) override{};
    cstring get_static_type() const override { return "PacketIn"; }
    cstring to_string() const override {
        cstring ret = "PacketIn(";
        ret += ")";
        return ret;
    }
    const IR::Method *get_function(cstring method_name) const {
        if (methods.count(method_name) > 0) {
            return methods.at(method_name);
        }
        error("Extern %s has no method %s.", p4_type, method_name);
        exit(1);
    }
    // TODO: This is a little pointless....
    PacketIn *copy() const override {
        return new PacketIn(state, extern_type);
    }
    P4Z3Instance *cast_allocate(const IR::Type *dest_type) const override;
};

}  // namespace TOZ3
