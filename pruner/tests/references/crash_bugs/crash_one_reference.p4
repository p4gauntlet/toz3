#include <core.p4>

bit<3> max(in bit<3> val) {
    return val;
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
    bool flag = true;
    bit<32> value = 32w10;
    bit<128> value_0 = 128w10;
    bit<64> value_1 = 64w10;
    bit<128> value_2 = value_0;
    return 16w10;
}
yLNoil wClaYko() {
    bit<4> value_3 = 4w14;
    const zRDuhm constant = (zRDuhm){LEQY = 128w232569008864530135157127505601818385808,KOcO = 8w10,EWpj = (ethernet_t){dst_addr = 48w211703505640505,src_addr = 48w10,eth_type = 16w10},BUWO = 32w10,NnjD = 32w2274055191};
    return (yLNoil){ILBe = 128w10,MXzB = 64w10};
}
bit<4> rjsgGkN() {
    const bit<128> constant_0 = 128w10;
    bit<16> value_4 = (bit<16>)16w10;
    return 4w10;
    bit<8> value_5 = (bit<8>)8w143;
    bit<16> value_6 = 16w46085;
    return 4w12;
}
parser p(packet_in pkt, out Headers hdr) {
    bit<4> value_7 = 4w0;
    bit<4> value_8 = 4w14;
    yLNoil hdr_0 = (yLNoil){ILBe = 128w10,MXzB = 64w8739799105651448972};
    bool flag_0 = false;
    bit<8> value_9 = 8w38;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<128> value_10 = 128w230764102599282411499082228184235706426;
    action AWhhg() {
    }
    action TyIkj() {
        const bool constant_1 = false;
        return;
        const bit<32> constant_2 = 32w10;
        return;
    }
    action isfLJ(bit<16> pxcw) {
        {
        }
        {
            avXIXF data = (avXIXF){Vpbo = 64w10,CeRJ = 32w10,tkyq = (zRDuhm){LEQY = 128w10,KOcO = (bit<8>)h.uAdg.HQZo,EWpj = (ethernet_t){dst_addr = 48w10,src_addr = 48w10,eth_type = pxcw},BUWO = 32w502793114,NnjD = 32w10}};
        }
        const gPTIia constant_3 = (gPTIia){pvdW = 32w10,HQZo = 8w10};
    }
    table xLdpWu {
        key = {
            16w19744: exact @name("wvtOpl");
        }
        actions = {
            isfLJ();
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table uiTHkJ {
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table YfGdXm {
        actions = {
            isfLJ();
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table ObUSpl {
        key = {
            4w9               : exact @name("LzviDL");
            16w19115          : exact @name("SBnBQt");
            48w145114785635731: exact @name("JEgqHq");
        }
        actions = {
            isfLJ();
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        const int constant_4 = 181433741;
        bit<128> value_11 = 128w334026077963046502987531789306364882607;
        const bit<4> constant_5 = 4w10;
        h.gvFM[max((bit<3>)SaTkzAX({ 4w6, 32w10, 16w10 }))].src_addr = 48w10;
        bit<64> value_12 = 64w10;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;
