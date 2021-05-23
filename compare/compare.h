#ifndef TOZ3_COMPARE_COMPARE_H_
#define TOZ3_COMPARE_COMPARE_H_

#include <utility>
#include <vector>

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "options.h"

namespace TOZ3 {
using Z3Prog = std::pair<cstring, std::vector<std::pair<cstring, z3::expr>>>;
constexpr auto COLUMN_WIDTH = 40;
int process_programs(const std::vector<cstring> &prog_list,
                     ParserOptions *options);

}  // namespace TOZ3

#endif  // TOZ3_COMPARE_COMPARE_H_
