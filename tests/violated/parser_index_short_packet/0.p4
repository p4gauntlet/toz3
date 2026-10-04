#include <core.p4>
header H { bit<8> next; }
struct Headers { H[1] stack; }
parser P(packet_in packet, out Headers headers) {
    state start {
        packet.extract(headers.stack[0]);
        transition select(headers.stack[0].next) {
            0: overflow;
            default: accept;
        }
    }
    state overflow {
        verify(false, error.StackOutOfBounds);
        transition reject;
    }
}
parser Proto(packet_in packet, out Headers headers);
package Top(Proto p);
Top(P()) main;
