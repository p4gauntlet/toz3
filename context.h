#ifndef _TOZ3_CONTEXT_H_
#define _TOZ3_CONTEXT_H_

#include <z3++.h>

#include <map>
#include <vector>

#include "boost/any.hpp"

#include "ir/ir.h"

namespace TOZ3_V2 {

class P4Scope {
 public:
    // a map of local values
    std::map<cstring, boost::any> value_map;
};
} // namespace TOZ3_V2

#endif // _TOZ3_CONTEXT_H_
