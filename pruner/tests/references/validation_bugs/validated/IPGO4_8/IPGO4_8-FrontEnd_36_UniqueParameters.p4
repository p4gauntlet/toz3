#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    bit<3> tmp;
    if (val < bound) {
        tmp = val;
    } else {
        tmp = bound;
    }
    return tmp;
}
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
    @name("UpwODq") bit<8> UpwODq_0;
    @name("GWcKTI") bit<64> GWcKTI_0;
    @name("aVQXxV") bit<4> aVQXxV_0;
    bit<3> tmp_0;
    bit<3> tmp_1;
    bit<3> tmp_2;
    bit<3> tmp_3;
    bit<64> tmp_4;
    bit<3> tmp_5;
    bit<3> tmp_6;
    @name("oQUdL") action oQUdL_0(aGXbEN Areb_0) {
        ;
        if (!Areb_0.Wfkd) {
            ;
        } else {
            tmp_0 = max((bit<3>)h.Wedn.eth_type, 3w1);
            tmp_1 = tmp_0;
            h.klMg[tmp_1].eth_type = h.Wedn.eth_type;
        }
        tmp_2 = max(3w0, 3w1);
        tmp_3 = tmp_2;
        h.klMg[tmp_3].dst_addr = h.CrEE.dst_addr;
    }
    apply {
        ;
        if ((bit<5>)h.klMg[1].dst_addr == 5w5) {
            ;
        } else {
            ;
        }
        ;
        ;
        tmp_5 = max(3w1, 3w1);
        tmp_6 = tmp_5;
        h.klMg[tmp_6].eth_type = 16w33216;
        ;
        oQUdL_0((aGXbEN){yTnv = 128w340282366920938463463374607431595471583,Psna = 4w12,Wfkd = true,MZDy = 8w41,rkyg = 64w18446744073709551615});
        ;
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

