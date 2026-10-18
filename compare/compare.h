#ifndef TOZ3_COMPARE_COMPARE_H_
#define TOZ3_COMPARE_COMPARE_H_

#include <z3++.h>

#include <utility>
#include <vector>

#include "frontends/common/parser_options.h"
#include "lib/cstring.h"

namespace P4::ToZ3 {
struct Z3Prog {
    cstring name;
    std::vector<std::pair<cstring, z3::expr>> outputs;
    z3::expr input_constraints;
};
constexpr auto COLUMN_WIDTH = 40;
z3::expr defined_when(const z3::expr &expression);
int process_programs(const std::vector<std::filesystem::path> &prog_list, ParserOptions *options,
                     bool allow_undefined = false, bool diagnostics = true);

}  // namespace P4::ToZ3

#endif  // TOZ3_COMPARE_COMPARE_H_
