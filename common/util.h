#ifndef TOZ3_COMMON_UTIL_H_
#define TOZ3_COMMON_UTIL_H_

#include <climits>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <string>

#include <boost/format.hpp>

#include "ir/ir.h"
#include "lib/cstring.h"
#include "lib/log.h"

// The program is empty, there is nothing to do here
#define EXIT_SKIPPED 10
// When comparing two programs, they are not equal
#define EXIT_VIOLATION 20
// Comparing two programs, they have undefined behavior that makes them unequal
#define EXIT_UNDEF 30

#define UNDEF_LABEL "undefined"
#define INVALID_LABEL "invalid"

#ifndef LOG_LEVEL
#define LOG_LEVEL 1
#endif

// If INT_WIDTH is not defined, define it as the size of int
#ifndef INT_WIDTH
#define INT_WIDTH sizeof(int)
#endif

namespace TOZ3 {

cstring get_max_bv_val(uint64_t bv_width);
cstring infer_name(const IR::Annotations *annots, cstring default_name);
bool compare_files(const cstring &filename1, const cstring &filename2);
int exec(const char *cmd, std::stringstream &output);

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
        std::initializer_list<char>{(static_cast<void>(f % args), char{})...};  // NOLINT
#pragma GCC diagnostic pop
        LOGN(level, boost::str(f));
    }
};

}  // namespace TOZ3

#endif  // TOZ3_COMMON_UTIL_H_
