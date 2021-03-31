#ifndef _TOZ3_COMPARE_OPTIONS_H_
#define _TOZ3_COMPARE_OPTIONS_H_

#include "ir/ir.h"

#include "frontends/common/options.h"
#include "lib/options.h"

namespace TOZ3_V2 {

class CompareOptions : public CompilerOptions {
 public:
    CompareOptions();
    // The P4 program we want to compare against
    cstring compare_file = nullptr;

};

using P4toZ3Context = P4CContextWithOptions<CompareOptions>;

} // namespace TOZ3_V2

#endif /* _TOZ3_COMPARE_OPTIONS_H_ */
