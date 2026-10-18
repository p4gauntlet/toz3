#include <core.p4>
@command_line("--loopsUnroll")
header H { bit<8> next; }
struct Headers { H[1] stack; }
parser P(packet_in packet, out Headers headers) {
    int<32> index;
    state start { index = 0; transition again; }
    state again {
        packet.extract(headers.stack[index]);
        index += 1;
        transition select(headers.stack[index - 1].next) {
            0: again;
            default: accept;
        }
    }
}
parser Proto(packet_in packet, out Headers headers);
package Top(Proto p);
Top(P()) main;
