#include <core.p4>

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

struct aGXbEN {
    bit<128> yTnv;
    bit<4>   Psna;
    bool     Wfkd;
    bit<8>   MZDy;
    bit<64>  rkyg;
}

struct Headers {
    ethernet_t    eth_hdr;
    ethernet_t    Wedn;
    ethernet_t    KZmt;
    ethernet_t[2] klMg;
    ethernet_t    CrEE;
}

parser p(packet_in pkt, out Headers hdr) {
    state start {
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<3> tmp_0;
    bit<3> tmp_1;
    @name("oQUdL") action oQUdL_0(aGXbEN Areb_0) {
        if (!Areb_0.Wfkd) {
            ;
        } else {
            {
                bit<3> val_0 = 3w2;
                bit<3> bound_0 = 3w1;
                bool hasReturned = false;
                bit<3> retval;
                bit<3> tmp;
                if (val_0 < 3w2) {
                    tmp = 3w2;
                } else {
                    tmp = bound_0;
                }
                hasReturned = true;
                retval = tmp;
                tmp_0 = retval;
            }
            tmp_1 = tmp_0;
            h.klMg[tmp_1].eth_type = h.Wedn.eth_type;
        }
    }
    apply {
        oQUdL_0((aGXbEN){yTnv = 128w10,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w0});
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

