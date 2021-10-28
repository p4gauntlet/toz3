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
    aGXbEN Areb;
    @name("ingress.oQUdL") action oQUdL() {
        {
            Areb.yTnv = 128w10;
            Areb.Psna = 4w12;
            Areb.Wfkd = true;
            Areb.MZDy = 8w41;
            Areb.rkyg = 64w0;
        }
        {
            bool cond;
            cond = !Areb.Wfkd;
            {
                {
                    {
                        bool cond_0;
                        cond_0 = val_0 < 3w2;
                        val_0 = (cond ? val_0 : 3w2);
                        bound_0 = (cond ? bound_0 : 3w1);
                        hasReturned = (cond ? hasReturned : false);
                        tmp_1 = (cond ? tmp_1 : (cond_0 ? 3w2 : tmp_1));
                        tmp_1 = (cond ? tmp_1 : (cond_0 ? 3w2 : bound_0));
                    }
                }
            }
            hasReturned = (cond ? hasReturned : true);
            retval = (cond ? retval : tmp_1);
            tmp = (cond ? tmp : retval);
            tmp_0 = (cond ? tmp_0 : tmp);
            h.klMg[tmp_0].eth_type = (cond ? h.klMg[tmp_0].eth_type : h.Wedn.eth_type);
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

