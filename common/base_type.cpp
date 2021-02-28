#include "base_type.h"

namespace TOZ3_V2 {

z3::expr P4Z3Instance::operator==(const P4Z3Instance &) {
    BUG("Eq not implemented for %s.", get_static_type());
}

z3::expr P4Z3Instance::operator!() { BUG("Lnot not implemented."); }
z3::expr P4Z3Instance::operator!() const { BUG("Lnot not implemented."); }

void P4Z3Instance::merge(z3::expr *, const P4Z3Instance *) {
    BUG("Complex expression merge not implemented for %s.", get_static_type());
}
std::vector<std::pair<cstring, z3::expr>> P4Z3Instance::get_z3_vars() const {
    BUG("Get Z3 vars not implemented for %s.", get_static_type());
}

} // namespace TOZ3_V2
