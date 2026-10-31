// SPDX-FileCopyrightText: 2026 The P4 Language Consortium
// SPDX-License-Identifier: Apache-2.0

#include <core.p4>
#include <v1model.p4>

header ByteHeader { bit<8> value; }
struct Headers { ByteHeader data; }
struct Metadata { }

T identity<T>(in T value) { return value; }

parser OwnershipParser(packet_in packet, out Headers hdr, inout Metadata meta,
              inout standard_metadata_t standard) {
    state start {
        packet.extract(hdr.data);
        transition accept;
    }
}

control OwnershipIngress(inout Headers hdr, inout Metadata meta,
                inout standard_metadata_t standard) {
    action increment() {
        hdr.data.value = identity<bit<8>>(hdr.data.value + 1);
    }
    apply {
        increment();
        standard.egress_spec = 1;
    }
}
control OwnershipEgress(inout Headers hdr, inout Metadata meta,
               inout standard_metadata_t standard) { apply { } }
control Verify(inout Headers hdr, inout Metadata meta) { apply { } }
control Update(inout Headers hdr, inout Metadata meta) { apply { } }
control OwnershipDeparser(packet_out packet, in Headers hdr) {
    apply { packet.emit(hdr.data); }
}
V1Switch(OwnershipParser(), Verify(), OwnershipIngress(), OwnershipEgress(), Update(), OwnershipDeparser()) main;
