#include <core.p4>

bit<3> max(in bit<3> val) {
    return 3w2;
}
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

header xWYJTN {
    bit<8> astn;
    bit<8> kyes;
    bit<4> eyvp;
}

struct Headers {
    ethernet_t eth_hdr;
    gshtZr     JQlW;
    gshtZr     szOQ;
    ethernet_t YYSU;
    gshtZr[6]  XgXd;
}

bit<16> zvVNECN(gshtZr jFuj) {
    return 16w10;
    return 16w10;
}
parser p(packet_in pkt, out Headers hdr) {
    bit<128> value = 128w10;
    bit<16> value_0 = hdr.YYSU.eth_type;
    bit<128> value_1 = 128w10;
    bool flag = false;
    bool flag_0 = true;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    ethernet_t hdr_0 = h.eth_hdr;
    action dUbOl() {
        const bit<32> constant = 32w10;
        bit<32> value_2 = 32w10;
        h.XgXd[max((bit<3>)zvVNECN({ 32w10, 64w13544346735733648404, 32w10, 64w10, 16w23806 }))].RbAp = 32w10;
    }
    table RGCyPy {
        key = {
            h.XgXd[5].SwvD : exact @name("YQNeBH");
            h.JQlW.gpRw    : exact @name("ifCVyt");
            h.YYSU.src_addr: exact @name("yaGJNK");
        }
        actions = {
            dUbOl();
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        bool flag_1 = true;
        bit<4> value_3 = (bit<4>)hdr_0.eth_type;
        bit<64> value_4 = 64w10;
        const int constant_0 = 238813955;
        const bit<128> constant_1 = 128w10;
        bit<8> value_5 = 8w10;
        {
            const bit<16> constant_2 = 16w10;
            const bit<32> constant_3 = 32w10;
        }
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;
