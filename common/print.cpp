
#include "ir/ir.h"
#include "lib/cstring.h"

using namespace TOZ3_V2;

std::ostream &operator<<(std::ostream &out, const Z3Int &instance) {
    out << "Z3Int(" << instance.val << ")";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Z3Wrapper &instance) {
    out << instance.val;
    return out;
}
