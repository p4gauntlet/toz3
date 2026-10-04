#include <core.p4>
header A { bit<8> x; }
header B { bit<8> x; varbit<8> data; }
header_union U { A a; B b; }
struct Headers { U[1] stack; }
parser P(packet_in packet, out Headers headers) {
    state start {
        packet.extract(headers.stack.next.a);
        packet.extract(headers.stack.next.b, (bit<32>) packet.lookahead<bit<8>>());
        transition accept;
    }
}
parser Proto(packet_in packet, out Headers headers);
package Top(Proto p);
Top(P()) main;
