#include <core.p4>
header H { bit<8> x; }
header_union U { H a; H b; }
struct S { U u; }
control C(in S src, out S dst) {
    apply { dst.u = src.u; }
}
control Proto(in S src, out S dst);
package Top(Proto c);
Top(C()) main;
