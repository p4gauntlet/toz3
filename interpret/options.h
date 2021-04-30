#ifndef TOZ3_INTERPRET_OPTIONS_H_
#define TOZ3_INTERPRET_OPTIONS_H_

#include "ir/ir.h"

#include "frontends/common/options.h"
#include "lib/options.h"

namespace TOZ3 {

class toz3Options : public CompilerOptions {
 public:
    toz3Options();
};

using P4toZ3Context = P4CContextWithOptions<toz3Options>;

}  // namespace TOZ3

#endif  // TOZ3_INTERPRET_OPTIONS_H_
