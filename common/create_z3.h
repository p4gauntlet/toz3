#ifndef TOZ3_COMMON_CREATE_Z3_H_
#define TOZ3_COMMON_CREATE_Z3_H_

#include <map>      // std::map
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "visitor_interpret.h"

using MainResult =
    std::map<cstring, std::pair<std::vector<std::pair<cstring, z3::expr>>,
                                const IR::Type *>>;
namespace TOZ3 {
MainResult gen_state_from_instance(Z3Visitor *visitor,
                                   const IR::Declaration_Instance *di);

}  // namespace TOZ3

#endif  // TOZ3_COMMON_CREATE_Z3_H_
