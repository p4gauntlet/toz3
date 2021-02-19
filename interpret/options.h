#ifndef _TOZ3_INTERPRET_OPTIONS_H_
#define _TOZ3_INTERPRET_OPTIONS_H_

#include "ir/ir.h"

#include "frontends/common/options.h"
#include "lib/options.h"

namespace TOZ3_V2 {

class toz3Options : public CompilerOptions {
 public:
    toz3Options();
};

using P4toZ3Context = P4CContextWithOptions<toz3Options>;

} // namespace TOZ3_V2

#endif /* _TOZ3_INTERPRET_OPTIONS_H_ */
