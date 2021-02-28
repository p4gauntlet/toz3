#ifndef _TOZ3_PRINT_H_
#define _TOZ3_PRINT_H_

#include <z3++.h>

#include <cstdio>

#include <map>    // std::map
#include <vector> // std::vector

#include "ir/ir.h"
#include "lib/cstring.h"

#include "scope.h"

std::ostream &operator<<(std::ostream &out, const TOZ3_V2::Z3Int &instance);

std::ostream &operator<<(std::ostream &out, const TOZ3_V2::Z3Wrapper &instance);

std::ostream &operator<<(std::ostream &out, const TOZ3_V2::P4Z3Instance &type);

// Some print definitions
std::ostream &operator<<(std::ostream &out,
                         const TOZ3_V2::StructBase &instance);

std::ostream &operator<<(std::ostream &out, const TOZ3_V2::P4Scope *scope);

std::ostream &operator<<(std::ostream &out, const TOZ3_V2::P4Scope &scope);

#endif // _TOZ3_PRINT_H_
