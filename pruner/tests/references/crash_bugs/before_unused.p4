#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    return 3w2;
}
header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

struct zRDuhm {
    bit<128>   LEQY;
    bit<8>     KOcO;
    ethernet_t EWpj;
    bit<32>    BUWO;
    bit<32>    NnjD;
}

header PguzcJ {
    bit<4>  YRcl;
    bit<32> vGcj;
    bit<16> CazW;
}

header yLNoil {
    bit<128> ILBe;
    bit<64>  MXzB;
}

header vuOVMH {
    bit<8>   GPVl;
    bit<4>   hrds;
    bit<128> HoON;
    bit<32>  Wsxt;
}

struct avXIXF {
    bit<64> Vpbo;
    bit<32> CeRJ;
    zRDuhm  tkyq;
}

header gPTIia {
    bit<32> pvdW;
    bit<8>  HQZo;
}

struct OXxQuH {
    bit<8> xBZh;
}

header jUHTpn {
    bit<4>  yrVX;
    bit<32> bZKC;
}

struct Headers {
    ethernet_t    eth_hdr;
    ethernet_t[6] gvFM;
    gPTIia        uAdg;
    PguzcJ        IDZn;
    PguzcJ        zoNJ;
    gPTIia        oWtO;
}

bit<16> SaTkzAX(PguzcJ Oofi) {
    bool cmdluC = true;
    bit<32> cqckxY = Oofi.vGcj;
    bit<128> RfSkhs = 128w10;
    bit<64> nZqXev = 64w10;
    bit<128> xYhUqh = RfSkhs;
    return 16w10;
}
yLNoil wClaYko(jUHTpn DoHl) {
    bit<4> wgIKEQ = 4w10;
    const zRDuhm zTQmam = (zRDuhm){LEQY = 128w10,KOcO = 8w10,EWpj = (ethernet_t){dst_addr = 48w10,src_addr = 48w13821584846488,eth_type = 16w5400},BUWO = 32w10,NnjD = 32w10};
    return (yLNoil){ILBe = 128w10,MXzB = 64w10};
}
bit<4> rjsgGkN(bit<16> SOZw) {
    const bit<128> cMIrQM = 128w159430385411339469055938130538993747062;
    bit<16> YVHHin = 16w10;
    return 4w10;
    bit<8> XmGxDm = 8w10;
    bit<16> aneguV = 16w10;
    return 4w12;
}
parser p(packet_in pkt, out Headers hdr) {
    bit<4> WCyPOo = 4w0;
    bit<4> OpKEBC = 4w10;
    yLNoil LnJSsf = (yLNoil){ILBe = 128w10,MXzB = 64w10};
    bool MHFZYM = false;
    bit<8> AfRsJs = 8w10;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<128> WbiJDx = 128w10;
    action AWhhg(inout bit<16> HpDQ, OXxQuH GSUK) {
    }
    action TyIkj(bit<8> cFUf, bit<64> UYcs) {
        const bool naGUDB = false;
        return;
        const bit<32> kFhgzn = 32w10;
        return;
    }
    action isfLJ(inout bit<4> zayb, bit<32> ezim, bit<16> pxcw) {
        {
        }
        {
            avXIXF paUCEG = (avXIXF){Vpbo = 64w10,CeRJ = 32w10,tkyq = (zRDuhm){LEQY = 128w10,KOcO = 8w10,EWpj = (ethernet_t){dst_addr = 48w10,src_addr = 48w10,eth_type = pxcw},BUWO = 32w10,NnjD = 32w10}};
        }
        const gPTIia khmurm = (gPTIia){pvdW = 32w10,HQZo = 8w251};
    }
    table xLdpWu {
        key = {
            16w10: exact @name("wvtOpl") ;
        }
        actions = {
            isfLJ(h.IDZn.YRcl);
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table uiTHkJ {
        key = {
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table YfGdXm {
        key = {
        }
        actions = {
            isfLJ(h.uAdg.pvdW[20:17]);
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table ObUSpl {
        key = {
            4w10 : exact @name("LzviDL") ;
            16w10: exact @name("SBnBQt") ;
            48w10: exact @name("JEgqHq") ;
        }
        actions = {
            isfLJ(h.IDZn.YRcl);
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        const int XsZqcA = 181433741;
        bit<128> CYNOag = 128w10;
        const bit<4> lgWdgD = 4w10;
        h.gvFM[max((bit<3>)SaTkzAX({ 4w10, 32w10, 16w30993 }), 3w2)].src_addr = h.gvFM[5].src_addr;
        bit<64> GTZTgt = 64w10;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;

