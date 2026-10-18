#include <core.p4>
header H { bit<8> x; }
header_union U { H a; H b; }
struct S { U u; }
parser P(packet_in packet, inout S s) {
    state start { packet.extract(s.u.a); transition accept; }
}
parser Proto(packet_in packet, inout S s);
package Top(Proto p);
Top(P()) main;
