#ifndef TOZ3_COMPARE_COMPARE_H_
#define TOZ3_COMPARE_COMPARE_H_

#include <utility>
#include <vector>

#include "../contrib/z3/z3++.h"
#include "frontends/common/parser_options.h"
#include "lib/cstring.h"

namespace TOZ3 {
using Z3Prog = std::pair<cstring, std::vector<std::pair<cstring, z3::expr>>>;
constexpr auto COLUMN_WIDTH = 40;
int process_programs(const std::vector<cstring> &prog_list, ParserOptions *options,
                     bool allow_undefined = false);

}  // namespace TOZ3

#endif  // TOZ3_COMPARE_COMPARE_H_
