#ifndef TOZ3_COMPARE_OPTIONS_H_
#define TOZ3_COMPARE_OPTIONS_H_

#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"

namespace TOZ3 {

class CompareOptions : public CompilerOptions {
 public:
    CompareOptions();
    // Toggle this to allow differences in undefined behavior.
    bool undefined_is_ok = false;
};

using P4toZ3Context = P4CContextWithOptions<CompareOptions>;

}  // namespace TOZ3

#endif  // TOZ3_COMPARE_OPTIONS_H_
