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
    @name("ingress.val_0") bit<3> val_0;
    @name("ingress.bound_0") bit<3> bound_0;
    @name("ingress.hasReturned") bool hasReturned;
    @name("ingress.retval") bit<3> retval;
    @name("ingress.tmp") bit<3> tmp_6;
    @name("ingress.tmp") bit<3> tmp_14;
    bool cond_0;
    @name("ingress.oQUdL") action oQUdL() {
        {
            {
                {
                    {
                        cond_0 = val_0 < bound_0;
                        val_0 = (bit<3>)h.Wedn.eth_type;
                        bound_0 = 3w1;
                        hasReturned = false;
                        tmp_6 = (cond_0 ? val_0 : tmp_6);
                        tmp_6 = (cond_0 ? val_0 : bound_0);
                    }
                }
            }
            hasReturned = true;
            retval = tmp_6;
            tmp = retval;
            tmp_0 = tmp;
            h.klMg[tmp_0].eth_type = h.Wedn.eth_type;
        }
        h.klMg[3w0].dst_addr = h.CrEE.dst_addr;
    }
    apply {
        {
            tmp_14 = 3w1;
        }
        h.klMg[tmp_14].eth_type = 16w33216;
        oQUdL();
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

