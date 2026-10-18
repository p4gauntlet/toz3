#include <core.p4>
header H { bit<8> x; }
header_union U { H a; H b; }
control C(inout U u) {
    apply { if (u.a.isValid()) { u.a.x = 2; } }
}
control Proto(inout U u);
package Top(Proto c);
Top(C()) main;
