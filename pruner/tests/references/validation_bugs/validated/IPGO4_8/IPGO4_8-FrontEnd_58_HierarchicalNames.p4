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
    @name("ingress.tmp_0") bit<3> tmp;
    @name("ingress.tmp_1") bit<3> tmp_0;
    @name("ingress.tmp_2") bit<3> tmp_1;
    @name("ingress.tmp_3") bit<3> tmp_2;
    @name("ingress.tmp_5") bit<3> tmp_3;
    @name("ingress.tmp_6") bit<3> tmp_5;
    @name("ingress.oQUdL") action oQUdL(aGXbEN Areb) {
        if (!Areb.Wfkd) {
            ;
        } else {
            {
                @name("ingress.val_0") bit<3> val_0 = (bit<3>)h.Wedn.eth_type;
                @name("ingress.bound_0") bit<3> bound_0 = 3w1;
                @name("ingress.hasReturned") bool hasReturned = false;
                @name("ingress.retval") bit<3> retval;
                @name("ingress.tmp") bit<3> tmp_6;
                if (val_0 < bound_0) {
                    tmp_6 = val_0;
                } else {
                    tmp_6 = bound_0;
                }
                hasReturned = true;
                retval = tmp_6;
                tmp = retval;
            }
            tmp_0 = tmp;
            h.klMg[tmp_0].eth_type = h.Wedn.eth_type;
        }
        {
            @name("ingress.val_1") bit<3> val_1 = 3w0;
            @name("ingress.bound_1") bit<3> bound_1 = 3w1;
            @name("ingress.hasReturned") bool hasReturned_1 = false;
            @name("ingress.retval") bit<3> retval_1;
            @name("ingress.tmp") bit<3> tmp_13;
            if (val_1 < bound_1) {
                tmp_13 = val_1;
            } else {
                tmp_13 = bound_1;
            }
            hasReturned_1 = true;
            retval_1 = tmp_13;
            tmp_1 = retval_1;
        }
        tmp_2 = tmp_1;
        h.klMg[tmp_2].dst_addr = h.CrEE.dst_addr;
    }
    apply {
        {
            @name("ingress.val_2") bit<3> val_2 = 3w1;
            @name("ingress.bound_2") bit<3> bound_2 = 3w1;
            @name("ingress.hasReturned") bool hasReturned_2 = false;
            @name("ingress.retval") bit<3> retval_2;
            @name("ingress.tmp") bit<3> tmp_14;
            if (val_2 < bound_2) {
                tmp_14 = val_2;
            } else {
                tmp_14 = bound_2;
            }
            hasReturned_2 = true;
            retval_2 = tmp_14;
            tmp_3 = retval_2;
        }
        tmp_5 = tmp_3;
        h.klMg[tmp_5].eth_type = 16w33216;
        oQUdL((aGXbEN){yTnv = 128w340282366920938463463374607431595471583,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w18446744073709551615});
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

