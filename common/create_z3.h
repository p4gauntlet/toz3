#ifndef TOZ3_COMMON_CREATE_Z3_H_
#define TOZ3_COMMON_CREATE_Z3_H_

#include "ir/ir.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {

MainResult gen_state_from_instance(Z3Visitor *visitor, const IR::Declaration_Instance *di);
const IR::Declaration_Instance *get_main_decl(P4State *state);

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_CREATE_Z3_H_
