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
    @name("tmp_0") bit<3> tmp_2;
    @name("tmp_1") bit<3> tmp_3;
    @name("oQUdL") action oQUdL(aGXbEN Areb_0) {
        if (!Areb_0.Wfkd) {
            ;
        } else {
            {
                @name("val_0") bit<3> val = 3w2;
                @name("bound_0") bit<3> bound = 3w1;
                @name("hasReturned") bool hasReturned_0 = false;
                @name("retval") bit<3> retval_0;
                @name("tmp") bit<3> tmp_4;
                if (val < 3w2) {
                    tmp_4 = 3w2;
                } else {
                    tmp_4 = bound;
                }
                hasReturned_0 = true;
                retval_0 = tmp_4;
                tmp_2 = retval_0;
            }
            tmp_3 = tmp_2;
            h.klMg[tmp_3].eth_type = h.Wedn.eth_type;
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

