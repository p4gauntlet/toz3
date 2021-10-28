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
    @name("ingress.tmp_0") bit<3> tmp;
    @name("ingress.tmp_1") bit<3> tmp_0;
    @name("ingress.val_0") bit<3> val_0;
    @name("ingress.bound_0") bit<3> bound_0;
    @name("ingress.hasReturned") bool hasReturned;
    @name("ingress.retval") bit<3> retval;
    @name("ingress.tmp") bit<3> tmp_1;
    bool cond_0;
    @name("ingress.oQUdL") action oQUdL() {
        {
            {
                {
                    {
                        cond_0 = val_0 < 3w2;
                        val_0 = (!true ? val_0 : 3w2);
                        bound_0 = (!true ? bound_0 : 3w1);
                        hasReturned = (!true ? hasReturned : false);
                        tmp_1 = (!true ? tmp_1 : (cond_0 ? 3w2 : tmp_1));
                        tmp_1 = (!true ? tmp_1 : (cond_0 ? 3w2 : bound_0));
                    }
                }
            }
            hasReturned = (!true ? hasReturned : true);
            retval = (!true ? retval : tmp_1);
            tmp = (!true ? tmp : retval);
            tmp_0 = (!true ? tmp_0 : tmp);
            h.klMg[tmp_0].eth_type = (!true ? h.klMg[tmp_0].eth_type : h.Wedn.eth_type);
        }
    }
    apply {
        oQUdL();
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

