#include <core.p4>
header H { bit<8> x; }
struct S { H u_a; H u_b; }
parser P(packet_in packet, inout S s) {
    state start { packet.extract(s.u_a); transition accept; }
}
parser Proto(packet_in packet, inout S s);
package Top(Proto p);
Top(P()) main;
