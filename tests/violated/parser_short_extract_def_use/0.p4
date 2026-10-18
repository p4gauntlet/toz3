#include <core.p4>
header H { bit<8> x; }
struct Headers { H first; H second; }
parser P(packet_in packet, out Headers headers) {
    state start {
        packet.extract(headers.first);
        packet.extract(headers.second);
        headers.first.x = 2;
        transition accept;
    }
}
parser Proto(packet_in packet, out Headers headers);
package Top(Proto p);
Top(P()) main;
