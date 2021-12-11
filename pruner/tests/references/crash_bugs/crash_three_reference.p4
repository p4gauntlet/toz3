#include <core.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

header vNUuQP {
    bit<8>  ETrI;
    bit<64> bUKc;
    bit<64> yxiq;
    bit<4>  JyBT;
}

header Mdpvgb {
    bit<8> TeIY;
    bit<4> LfLO;
}

struct Headers {
    ethernet_t    eth_hdr;
    vNUuQP        dSgK;
    vNUuQP        gUPl;
    Mdpvgb        AMIB;
    Mdpvgb[10]    vTHH;
    ethernet_t[9] aTcs;
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

