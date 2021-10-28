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
    @name("tmp_0") bit<3> tmp;
    @name("tmp_1") bit<3> tmp_0;
    @name("oQUdL") action oQUdL(aGXbEN Areb_0) {
        if (!Areb_0.Wfkd) {
            ;
        } else {
            {
                @name("val_0") bit<3> val_0 = 3w2;
                @name("bound_0") bit<3> bound_0 = 3w1;
                @name("hasReturned") bool hasReturned = false;
                @name("retval") bit<3> retval;
                @name("tmp") bit<3> tmp_1;
                if (val_0 < 3w2) {
                    tmp_1 = 3w2;
                } else {
                    tmp_1 = bound_0;
                }
                hasReturned = true;
                retval = tmp_1;
                tmp = retval;
            }
            tmp_0 = tmp;
            h.klMg[tmp_0].eth_type = h.Wedn.eth_type;
        }
    }
    apply {
        oQUdL((aGXbEN){yTnv = 128w10,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w0});
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

