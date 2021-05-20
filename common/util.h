#ifndef TOZ3_COMMON_UTIL_H_
#define TOZ3_COMMON_UTIL_H_
#include "ir/ir.h"

// The program is empty, there is nothing to do here
#define EXIT_SKIPPED 10
// When comparing two programs, they are unequal
#define EXIT_VIOLATION 20
// Comparing two programs, they have undefined behavior that makes them unequal
#define EXIT_UNDEF 30

#define UNDEF_LABEL "undefined"
#define INVALID_LABEL "invalid"

inline cstring get_max_bv_val(uint64_t bv_width) {
    big_int max_return = pow((big_int)2, bv_width) - 1;
    return Util::toString(max_return, 0, false);
}

namespace TOZ3 {
cstring infer_name(const IR::Annotations *annots, cstring default_name);
}  // namespace TOZ3

#endif  // TOZ3_COMMON_UTIL_H_
