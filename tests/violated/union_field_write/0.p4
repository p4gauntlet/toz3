#include <core.p4>
header H { bit<8> x; }
struct S { H u_a; H u_b; }
control C(inout S s) {
    apply {
        s.u_b.setValid();
        s.u_b.x = 2;
        s.u_a.setInvalid();
    }
}
control Proto(inout S s);
package Top(Proto c);
Top(C()) main;
