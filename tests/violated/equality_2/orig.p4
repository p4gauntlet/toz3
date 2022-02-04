#include <core.p4>
#include <v1model.p4>

header H {
    bit<8>     s;
    varbit<32> v;
}

header Same {
    bit<8> same;
}

struct metadata {
}

struct headers {
    H    h;
    H[2] a;
    Same same;
}

parser p(packet_in b, out headers hdr, inout metadata meta, inout standard_metadata_t stdmeta) {
    state start {
        b.extract<H>(hdr.h, 32w32);
        b.extract<H>(hdr.a.next, 32w32);
        b.extract<H>(hdr.a.next, 32w32);
        transition accept;
    }
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t stdmeta) {
    H[2] tmp_0;
    apply {
        hdr.same.setValid();
        hdr.same.same = 8w0;
        stdmeta.egress_spec = 9w0;
        if (hdr.h.s == hdr.a[0].s) {
            hdr.same.same = hdr.same.same | 8w1;
        }
        if (hdr.h.v == hdr.a[0].v) {
            hdr.same.same = hdr.same.same | 8w2;
        }
        if (!hdr.h.isValid() && !hdr.a[0].isValid() || hdr.h.isValid() && hdr.a[0].isValid() && hdr.h.s == hdr.a[0].s && hdr.h.v == hdr.a[0].v) {
            hdr.same.same = hdr.same.same | 8w4;
        }
        tmp_0[0] = hdr.h;
        tmp_0[1] = hdr.a[0];
        if (true && (!tmp_0[0].isValid() && !hdr.a[0].isValid() || tmp_0[0].isValid() && hdr.a[0].isValid() && tmp_0[0].s == hdr.a[0].s && tmp_0[0].v == hdr.a[0].v) && (!tmp_0[1].isValid() && !hdr.a[1].isValid() || tmp_0[1].isValid() && hdr.a[1].isValid() && tmp_0[1].s == hdr.a[1].s && tmp_0[1].v == hdr.a[1].v)) {
            hdr.same.same = hdr.same.same | 8w8;
        }
    }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t stdmeta) {
    apply {
    }
}

control vc(inout headers hdr, inout metadata meta) {
    apply {
    }
}

control uc(inout headers hdr, inout metadata meta) {
    apply {
    }
}

control deparser(packet_out packet, in headers hdr) {
    apply {
        {
            packet.emit<H>(hdr.h);
            packet.emit<H>(hdr.a[0]);
            packet.emit<H>(hdr.a[1]);
            packet.emit<Same>(hdr.same);
        }
    }
}

V1Switch<headers, metadata>(p(), vc(), ingress(), egress(), uc(), deparser()) main;
