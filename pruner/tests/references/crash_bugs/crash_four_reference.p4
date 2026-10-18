#include <core.p4>

bit<3> max(in bit<3> val, in bit<3> bound) {
    return bound;
}
header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> eth_type;
}

header bnsYVz {
    bit<128> jrPy;
    bit<64>  udHf;
    bit<8>   aPsC;
    bit<32>  wzfl;
}

struct umHNtO {
    bit<8> jXYS;
    bnsYVz SDFX;
}

struct VWxZaz {
    bit<4>  yJxL;
    bit<64> FnFc;
}

struct Headers {
    ethernet_t eth_hdr;
    bnsYVz[7]  Qcqz;
    bnsYVz     UHmR;
    bnsYVz     JfMU;
    ethernet_t YskI;
}

bit<16> vSDNsyx(in bit<32> saFq, VWxZaz vqBE) {
    bit<16> value = 16w10;
    return 16w10;
}
control XjhdqPA() {
    bit<4> value_0 = 4w10;
    bit<128> value_1 = 128w10;
    bit<32> value_2 = 32w10;
    bit<32> value_3 = 32w10;
    action ejnWB(bit<8> xpbU) {
        const bool constant = false;
    }
    table GhsaxE {
        key = {
            value_0: exact @name("lpWdPY");
            32w10  : exact @name("srdvlo");
            32w10  : exact @name("xIxLbp");
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table NZqfiU {
        key = {
            value_3: exact @name("PYKqvq");
            32w10  : exact @name("esQVok");
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table yvZmvC {
        key = {
            value_0: exact @name("Ehmics");
            128w10 : exact @name("ZonmhE");
        }
        actions = {
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        bit<8> value_4 = 8w10;
    }
}

parser p(packet_in pkt, out Headers hdr) {
    bit<128> value_5 = 128w10;
    bit<8> value_6 = hdr.Qcqz[6].aPsC;
    bit<16> value_7 = 16w10;
    umHNtO data = (umHNtO){jXYS = 8w104,SDFX = (bnsYVz){jrPy = 128w10,udHf = 64w10,aPsC = 8w10,wzfl = 32w10}};
    bool flag = false;
    state start {
        transition parse_hdrs;
    }
    state parse_hdrs {
        transition accept;
    }
}

control ingress(inout Headers h) {
    bit<64> value_8 = 64w10;
    bit<128> value_9 = 128w10;
    VWxZaz data_0 = (VWxZaz){yJxL = 4w10,FnFc = 64w10};
    bit<8> value_10 = 8w10;
    bit<8> value_11 = 8w10;
    XjhdqPA() sSDiPy;
    action fjxzW(out bit<16> VQxM, bit<32> DQDm) {
        bit<128> value_12 = 128w10;
        bit<16> value_13 = h.YskI.eth_type;
    }
    action QyHme(out bit<16> ecWy, bit<16> bZof) {
        h.Qcqz[max((bit<3>)vSDNsyx(32w10, { 4w10, 64w10 }), 3w2)].udHf = data_0.FnFc;
        const VWxZaz constant_0 = (VWxZaz){yJxL = 4w10,FnFc = 64w10};
        bit<16> value_14 = 16w10;
        bit<8> value_15 = value_11;
    }
    table KWzGQN {
        key = {
            32w10: exact @name("MarebU");
        }
        actions = {
            fjxzW(h.eth_hdr.eth_type);
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    table KWnxHC {
        key = {
            128w10: exact @name("rXqbdF");
        }
        actions = {
            QyHme(h.eth_hdr.eth_type);
            fjxzW(h.eth_hdr.eth_type);
            @defaultonly NoAction();
        }
        default_action = NoAction();
    }
    apply {
        bool flag_0 = false;
        const bit<16> constant_1 = 16w10;
    }
}

parser Parser(packet_in b, out Headers hdr);
control Ingress(inout Headers hdr);
package top(Parser p, Ingress ig);
top(p(), ingress()) main;
