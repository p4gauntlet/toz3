#ifndef TOZ3_VALIDATE_OPTIONS_H_
#define TOZ3_VALIDATE_OPTIONS_H_
#include <vector>

#include "ir/ir.h"

#include "frontends/common/options.h"

class ValidateOptions : public ParserOptions {
 private:
    static constexpr const char *defaultMessage = "Validate a P4 program";

 public:
    ValidateOptions();
    // Name of executable that is being run.
    cstring compiler_bin;
    cstring dump_dir;
};

using P4toZ3Context = P4CContextWithOptions<ValidateOptions>;

#endif  // TOZ3_VALIDATE_OPTIONS_H_
