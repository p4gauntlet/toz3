#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    return 3w2;
}
header ethernet_t {
}

header PguzcJ {
    bit<32> vGcj;
}

header gPTIia {
}

struct Headers {
    ethernet_t[6] gvFM;
    gPTIia        uAdg;
    PguzcJ        IDZn;
}

bit<16> SaTkzAX(PguzcJ Oofi) {
    return 16w10;
}
parser p(packet_in pkt, out Headers hdr) {
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    apply {
        h.gvFM[max((bit<3>)SaTkzAX({ 4w10, 32w10, 16w30993 }), 3w2)].src_addr = h.gvFM[5].src_addr;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

