#include <core.p4>
header H { bit<8> x; }
header_union U { H a; H b; }
struct S { U u; }
control C(inout S s) {
    apply { s.u.b.setInvalid(); }
}
control Proto(inout S s);
package Top(Proto c);
Top(C()) main;
