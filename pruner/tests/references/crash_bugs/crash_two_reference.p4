#include <core.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

header gshtZr {
    bit<32> RbAp;
    bit<64> nYRO;
    bit<32> paNB;
    bit<64> gpRw;
    bit<16> SwvD;
}

struct Headers {
    ethernet_t eth_hdr;
    gshtZr     JQlW;
    gshtZr     szOQ;
    ethernet_t YYSU;
    gshtZr[6]  XgXd;
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
        {
        }
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

