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
        pkt.extract<ethernet_t>(hdr.eth_hdr);
        pkt.extract<ethernet_t>(hdr.Wedn);
        pkt.extract<ethernet_t>(hdr.KZmt);
        pkt.extract<ethernet_t>(hdr.klMg.next);
        pkt.extract<ethernet_t>(hdr.klMg.next);
        pkt.extract<ethernet_t>(hdr.CrEE);
        transition accept;
    }
}

control ingress(inout Headers h) {
    @name("tmp_0") bit<3> tmp_4;
    @name("tmp_1") bit<3> tmp_7;
    @name("tmp_2") bit<3> tmp_8;
    @name("tmp_3") bit<3> tmp_9;
    @name("tmp_5") bit<3> tmp_10;
    @name("tmp_6") bit<3> tmp_11;
    @name("oQUdL") action oQUdL_0(aGXbEN Areb_0) {
        if (!Areb_0.Wfkd) {
            ;
        } else {
            {
                @name("val_0") bit<3> val = (bit<3>)h.Wedn.eth_type;
                @name("bound_0") bit<3> bound = 3w1;
                @name("hasReturned") bool hasReturned_0 = false;
                @name("retval") bit<3> retval_0;
                @name("tmp") bit<3> tmp_12;
                if (val < bound) {
                    tmp_12 = val;
                } else {
                    tmp_12 = bound;
                }
                hasReturned_0 = true;
                retval_0 = tmp_12;
                tmp_4 = retval_0;
            }
            tmp_7 = tmp_4;
            h.klMg[tmp_7].eth_type = h.Wedn.eth_type;
        }
        {
            @name("val_1") bit<3> val_3 = 3w0;
            @name("bound_1") bit<3> bound_3 = 3w1;
            @name("hasReturned") bool hasReturned_0 = false;
            @name("retval") bit<3> retval_0;
            @name("tmp") bit<3> tmp_12;
            if (val_3 < bound_3) {
                tmp_12 = val_3;
            } else {
                tmp_12 = bound_3;
            }
            hasReturned_0 = true;
            retval_0 = tmp_12;
            tmp_8 = retval_0;
        }
        tmp_9 = tmp_8;
        h.klMg[tmp_9].dst_addr = h.CrEE.dst_addr;
    }
    apply {
        {
            @name("val_2") bit<3> val_4 = 3w1;
            @name("bound_2") bit<3> bound_4 = 3w1;
            @name("hasReturned") bool hasReturned_0 = false;
            @name("retval") bit<3> retval_0;
            @name("tmp") bit<3> tmp_12;
            if (val_4 < bound_4) {
                tmp_12 = val_4;
            } else {
                tmp_12 = bound_4;
            }
            hasReturned_0 = true;
            retval_0 = tmp_12;
            tmp_10 = retval_0;
        }
        tmp_11 = tmp_10;
        h.klMg[tmp_11].eth_type = 16w33216;
        oQUdL_0((aGXbEN){yTnv = 128w340282366920938463463374607431595471583,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w18446744073709551615});
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

