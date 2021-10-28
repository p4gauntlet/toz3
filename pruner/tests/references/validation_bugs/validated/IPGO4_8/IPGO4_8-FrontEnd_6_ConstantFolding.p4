#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    return (val < bound ? val : bound);
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

extern bool pYDTYni();
parser p(packet_in pkt, out Headers hdr) {
    bool tVkUnc = false;
    bit<16> LEtWGd = 16w4198 >> (bit<8>)hdr.klMg[1].eth_type;
    bool IqOPIa = tVkUnc;
    bool WGgmHh = true;
    bit<16> mjkawb = -491550735;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        pkt.extract(hdr.eth_hdr);
        pkt.extract(hdr.Wedn);
        pkt.extract(hdr.KZmt);
        pkt.extract(hdr.klMg.next);
        pkt.extract(hdr.klMg.next);
        pkt.extract(hdr.CrEE);
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<8> UpwODq = 8w141;
    bit<64> GWcKTI = (!(5w0 >> (bit<8>)(bit<5>)h.KZmt.eth_type | (bit<5>)h.klMg[1].dst_addr != 5w5) ? (bit<64>)UpwODq : 64w18446744073678420613);
    bit<4> aVQXxV = 4w9;
    action oQUdL(aGXbEN Areb) {
        const bit<4> Cuxfeu = 4w0;
        aVQXxV = ((bit<18>)aVQXxV)[7:4];
        const bool XdnskJ = false;
        const bit<4> XxvXpl = 4w4;
        if (!!!!(!Areb.Wfkd && true)) {
            GWcKTI = 64w0;
        } else {
            h.klMg[max((bit<3>)h.Wedn.eth_type, 3w1)].eth_type = h.Wedn.eth_type;
        }
        h.klMg[max(3w0, 3w1)].dst_addr = h.CrEE.dst_addr;
    }
    table wSZpOS {
        key = {
            UpwODq: exact @name("UvnjOY") ;
            4w6   : exact @name("djFEOc") ;
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table qFDvMl {
        key = {
            GWcKTI: exact @name("cxyXsi") ;
            aVQXxV: exact @name("cAikCN") ;
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table bWskDo {
        key = {
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        h.klMg[max(3w1, 3w1)].eth_type = 16w33216;
        aVQXxV = 4w14;
        oQUdL({ -172739873, 4w12, true, 8w41, 64w18446744073709551615 });
        ethernet_t qbQNsp = { 48w182822655386268, 48w25566851739804, 16w37651 |+| h.klMg[1].eth_type };
        GWcKTI = 64w12003250574396511282;
        h.eth_hdr.src_addr = 48w204206464434860;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

