#ifndef _TOZ3_OPTIONS_H_
#define _TOZ3_OPTIONS_H_

#include "ir/ir.h"

#include "frontends/common/options.h"
#include "lib/options.h"

namespace P4TOZ3 {

class toz3Options : public CompilerOptions {
  public:
    toz3Options();
};

using P4toZ3Context = P4CContextWithOptions<toz3Options>;

} // namespace P4TOZ3

#endif /* _TOZ3_OPTIONS_H_ */
