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
    bit<3> tmp_0;
    bit<3> tmp_1;
    bit<3> tmp_2;
    bit<3> tmp_3;
    bit<3> tmp_5;
    bit<3> tmp_6;
    @name("oQUdL") action oQUdL_0(aGXbEN Areb_0) {
        if (!Areb_0.Wfkd) {
            ;
        } else {
            {
                bit<3> val_0 = (bit<3>)h.Wedn.eth_type;
                bit<3> bound_0 = 3w1;
                bool hasReturned = false;
                bit<3> retval;
                bit<3> tmp;
                if (val_0 < bound_0) {
                    tmp = val_0;
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
        {
            bit<3> val_1 = 3w0;
            bit<3> bound_1 = 3w1;
            bool hasReturned = false;
            bit<3> retval;
            bit<3> tmp;
            if (val_1 < bound_1) {
                tmp = val_1;
            } else {
                tmp = bound_1;
            }
            hasReturned = true;
            retval = tmp;
            tmp_2 = retval;
        }
        tmp_3 = tmp_2;
        h.klMg[tmp_3].dst_addr = h.CrEE.dst_addr;
    }
    apply {
        {
            bit<3> val_2 = 3w1;
            bit<3> bound_2 = 3w1;
            bool hasReturned = false;
            bit<3> retval;
            bit<3> tmp;
            if (val_2 < bound_2) {
                tmp = val_2;
            } else {
                tmp = bound_2;
            }
            hasReturned = true;
            retval = tmp;
            tmp_5 = retval;
        }
        tmp_6 = tmp_5;
        h.klMg[tmp_6].eth_type = 16w33216;
        oQUdL_0((aGXbEN){yTnv = 128w340282366920938463463374607431595471583,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w18446744073709551615});
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

