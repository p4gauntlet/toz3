#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    return 3w2;
}
header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

struct NAZvKo {
    bool    PZbt;
    bool    vAYd;
    bit<16> HYBx;
    bit<32> XcOv;
}

header vNUuQP {
    bit<8>  ETrI;
    bit<64> bUKc;
    bit<64> yxiq;
    bit<4>  JyBT;
}

header Mdpvgb {
    bit<8> TeIY;
    bit<4> LfLO;
}

struct Headers {
    ethernet_t    eth_hdr;
    vNUuQP        dSgK;
    vNUuQP        gUPl;
    Mdpvgb        AMIB;
    Mdpvgb[10]    vTHH;
    ethernet_t[9] aTcs;
}

bit<8> TFhEvSU(in Mdpvgb Mkfw, out bit<4> VPgh, Mdpvgb kemL) {
    bit<16> value = 16w10;
    const bit<128> constant = 128w10;
    {
        const bit<16> constant_0 = 16w10;
        const bool constant_1 = false;
        bit<8> value_0 = 8w10;
        bool flag = false;
        return 8w10;
    }
    const int constant_2 = 113023712;
    return 8w10;
}
bool AuSgrSB() {
    bool flag_0 = true;
    const bool constant_3 = false;
    bool flag_1 = true;
    bit<8> value_1 = 8w10;
    vNUuQP hdr_0 = (vNUuQP){ETrI = value_1,bUKc = 64w10,yxiq = 64w10,JyBT = 4w10};
    return true;
}
bit<16> mMjjrfi(bit<32> XROz) {
    const bit<4> constant_4 = 4w10;
    {
        bool flag_2 = false;
        bool flag_3 = true;
        bit<8> value_2 = 8w10;
        ethernet_t hdr_1 = (ethernet_t){dst_addr = 48w10,src_addr = 48w10,eth_type = 16w10};
        return hdr_1.eth_type;
    }
    {
        Mdpvgb hdr_2 = (Mdpvgb){TeIY = 8w10,LfLO = 4w10};
    }
    {
        Mdpvgb hdr_3 = (Mdpvgb){TeIY = 8w10,LfLO = 4w10};
    }
    bit<128> value_3 = 128w279538546481246464944740951434909156558;
    bit<128> value_4 = 128w10;
    bit<128> value_5 = 128w10;
    return 16w10;
}
parser p(packet_in pkt, out Headers hdr) {
    bit<128> value_6 = 128w10;
    vNUuQP hdr_4 = (vNUuQP){ETrI = 8w10,bUKc = hdr.dSgK.bUKc,yxiq = 64w10,JyBT = 4w6};
    ethernet_t[2] hdrs;
    bool flag_4 = false;
    bit<64> value_7 = 64w11591036449157216331;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<32> value_8 = 32w10;
    bit<4> value_9 = 4w10;
    bit<4> value_10 = value_9;
    vNUuQP hdr_5 = (vNUuQP){ETrI = 8w10,bUKc = h.dSgK.bUKc,yxiq = 64w10,JyBT = h.gUPl.JyBT};
    action lBJGe(bit<4> VqqH, bit<128> mlYh) {
        bit<128> value_11 = 128w10;
    }
    action naNRp(inout bit<64> fyaf, out bit<32> OEEJ, bit<128> KZqQ) {
        bool flag_5 = true;
        h.aTcs[max((bit<3>)TFhEvSU((Mdpvgb){TeIY = h.vTHH[9].TeIY,LfLO = 4w10}, h.AMIB.LfLO, { 8w10, 4w10 }), 3w2)].src_addr = h.eth_hdr.src_addr;
        const bool constant_5 = false;
    }
    action TtncC(inout bit<4> ClFo, bit<8> xLMh) {
        return;
        const int constant_6 = 1563578230;
    }
    table LXUTiq {
        key = {
            48w237754727661431: exact @name("knrnEO");
            h.gUPl.bUKc       : exact @name("ZQHnkq");
            48w10             : exact @name("jbKdTS");
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        bit<32> value_12 = 32w10;
        bit<16> value_13 = 16w10;
        const bit<128> constant_7 = 128w10;
        bit<8> value_14 = h.vTHH[9].TeIY;
        bit<16> value_15 = 16w10;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;
