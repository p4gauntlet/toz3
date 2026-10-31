// SPDX-FileCopyrightText: 2026 The P4 Language Consortium
// SPDX-License-Identifier: Apache-2.0

#include <core.p4>
#include <v1model.p4>

header ByteHeader { bit<8> value; }
struct Headers { ByteHeader data; }
struct Metadata { }

parser SwitchParser(packet_in packet, out Headers hdr, inout Metadata meta,
              inout standard_metadata_t standard) {
    state start {
        packet.extract(hdr.data);
        transition accept;
    }
}

control SwitchIngress(inout Headers hdr, inout Metadata meta,
                inout standard_metadata_t standard) {
    apply {
        standard.egress_spec = 3;
        // Lowering this switch creates an immutable table. Exhausting its
        // entries must not make the last case match every remaining key.
        switch (hdr.data.value) {
            8w1: { standard.egress_spec = 1; }
            8w2: { standard.egress_spec = 2; }
        }
    }
}

control SwitchEgress(inout Headers hdr, inout Metadata meta,
               inout standard_metadata_t standard) { apply { } }
control Verify(inout Headers hdr, inout Metadata meta) { apply { } }
control Update(inout Headers hdr, inout Metadata meta) { apply { } }
control SwitchDeparser(packet_out packet, in Headers hdr) {
    apply { packet.emit(hdr.data); }
}
V1Switch(SwitchParser(), Verify(), SwitchIngress(), SwitchEgress(), Update(), SwitchDeparser()) main;
