#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
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

bit<16> zvVNECN(inout bit<64> UgWi, gshtZr jFuj) {
    return jFuj.SwvD;
    return 16w15386;
}
parser p(packet_in pkt, out Headers hdr) {
    bit<128> dXiaZd = 128w10;
    bit<16> nmvNXu = hdr.YYSU.eth_type;
    bit<128> myxqRj = 128w10;
    bool TzCKMV = false;
    bool bEVMPO = true;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    ethernet_t eFxdzn = h.eth_hdr;
    action dUbOl(bit<64> sLPS) {
        const bit<32> gZlvTF = 32w2278226877;
        bit<32> nBmBDv = 32w1130166714;
        h.XgXd[max((bit<3>)zvVNECN(h.szOQ.nYRO, { 32w708346825, 64w10, 32w512714473, 64w18310164472621113371, 16w23806 }), 3w5)].RbAp = 32w864455019;
    }
    table RGCyPy {
        key = {
            h.XgXd[5].SwvD : exact @name("YQNeBH") ;
            h.JQlW.gpRw    : exact @name("ifCVyt") ;
            h.YYSU.src_addr: exact @name("yaGJNK") ;
        }
        actions = {
            dUbOl();
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        bool Jvuqrm = true;
        bit<4> ZGknBz = (bit<4>)eFxdzn.eth_type;
        bit<64> VytFjf = 95w10[74:11];
        const int AXUyrM = 238813955;
        const bit<128> IcoSKn = 128w238813955;
        bit<8> OCNltr = 8w28;
        {
            const bit<16> QMseko = 16w10;
            const bit<32> mYhKao = 32w0;
        }
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

