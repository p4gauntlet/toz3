#ifndef TOZ3_INTERPRET_OPTIONS_H_
#define TOZ3_INTERPRET_OPTIONS_H_

#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"

namespace P4::ToZ3 {

class toz3Options : public CompilerOptions {
 public:
    toz3Options();
};

using P4toZ3Context = P4CContextWithOptions<toz3Options>;

}  // namespace P4::ToZ3

#endif  // TOZ3_INTERPRET_OPTIONS_H_
