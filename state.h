#ifndef _TOZ3_STATE_H_
#define _TOZ3_STATE_H_

#include <z3++.h>

#include <map>
#include <vector>

#include "scope.h"
#include "ir/ir.h"

namespace TOZ3_V2 {

class P4State {
 public:
    z3::context *ctx;
    P4State(z3::context *context) : ctx(context) {}

    z3::ast formula = ctx->bool_val(true);
    std::map<cstring, const IR::Type *> type_map;
    std::map<cstring, std::vector<const IR::Node *> *> z3_type_map;

    std::vector<P4Scope *> scopes;
    P4Z3Type gen_instance(cstring name, const IR::Type *typ, uint64_t id = 0);
    void add_scope(P4Scope *scope);
    const IR::Type *resolve_type(const IR::Type *type);
    void insert_var(cstring name, P4Z3Type var);
    P4Z3Type find_var(cstring name, P4Scope **owner_scope);
};
} // namespace TOZ3_V2

#endif // _TOZ3_STATE_H_

