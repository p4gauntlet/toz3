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
    // Name of compiler executable that is being tested.
    cstring compiler_bin;
    // Where the intermediate files are going to be dumped.
    cstring dump_dir;
    // Toggle this to allow differences in undefined behavior.
    bool undefined_is_ok = false;
};

using P4toZ3Context = P4CContextWithOptions<ValidateOptions>;

#endif  // TOZ3_VALIDATE_OPTIONS_H_
