#ifndef TOZ3_COMMON_UTIL_H_
#define TOZ3_COMMON_UTIL_H_

#include <string>

#include "ir/ir.h"

// The program is empty, there is nothing to do here
#define EXIT_SKIPPED 10
// When comparing two programs, they are unequal
#define EXIT_VIOLATION 20
// Comparing two programs, they have undefined behavior that makes them unequal
#define EXIT_UNDEF 30

#define UNDEF_LABEL "undefined"
#define INVALID_LABEL "invalid"

#ifndef LOG_LEVEL
#define LOG_LEVEL 1
#endif

namespace TOZ3 {

inline cstring get_max_bv_val(uint64_t bv_width) {
    big_int max_return = pow((big_int)2, bv_width) - 1;
    return Util::toString(max_return, 0, false);
}

cstring infer_name(const IR::Annotations *annots, cstring default_name);

class Logger {
 public:
    static void init() {
        cstring reg_str = cstring(__FILE__) + ":" + std::to_string(LOG_LEVEL);
        Log::addDebugSpec(reg_str.c_str());
    }
    template <typename... Args>
    static void log_msg(size_t level, const std::string &msg, Args &...args) {
        if (level > LOG_LEVEL) {
            return;
        }
        boost::format f(msg);
        std::initializer_list<char>{(static_cast<void>(f % args), char{})...};
        LOGN(level, boost::str(f));
    }
};

}  // namespace TOZ3

#endif  // TOZ3_COMMON_UTIL_H_
