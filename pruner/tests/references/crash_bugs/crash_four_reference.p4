#include <core.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

header bnsYVz {
    bit<128> jrPy;
    bit<64>  udHf;
    bit<8>   aPsC;
    bit<32>  wzfl;
}

struct Headers {
    ethernet_t eth_hdr;
    bnsYVz[7]  Qcqz;
    bnsYVz     UHmR;
    bnsYVz     JfMU;
    ethernet_t YskI;
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

