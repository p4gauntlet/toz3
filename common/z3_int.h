#ifndef _TOZ3_Z3_INT_H_
#define _TOZ3_Z3_INT_H_

#include <z3++.h>

#include "ir/ir.h"
#include "lib/gmputil.h"
#include "state.h"

namespace TOZ3_V2 {

class Z3Int : public P4ComplexInstance {
 public:
    z3::expr val;
    int64_t width;
    Z3Int(z3::expr val, int64_t width) : val(val), width(width){};
};

} // namespace TOZ3_V2

#endif // _TOZ3_Z3_INT_H_
