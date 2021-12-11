#include <core.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

header PguzcJ {
    bit<4>  YRcl;
    bit<32> vGcj;
    bit<16> CazW;
}

header gPTIia {
    bit<32> pvdW;
    bit<8>  HQZo;
}

struct Headers {
    ethernet_t    eth_hdr;
    ethernet_t[6] gvFM;
    gPTIia        uAdg;
    PguzcJ        IDZn;
    PguzcJ        zoNJ;
    gPTIia        oWtO;
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
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

