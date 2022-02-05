#include "include/size.p4"
#include "include/control/filtering.p4"
#include "include/checksum.p4"
#include "include/parser.p4"
 bit<16> a (inout ipv4_t b){                          return b.total_len;                      }
                         control c (inout parsed_headers_t d,                        inout fabric_metadata_t e,                        inout standard_metadata_t f                        ) {                         table g {                           key = {                           a(d.inner_ipv4): exact @name("");                        }                           actions = {     }                       }                          apply {                              g.apply();                       }                      }
                         control h (inout parsed_headers_t d,                       inout fabric_metadata_t e,                       inout standard_metadata_t f) {             apply {      }                      }
                         V1Switch(     FabricParser(),     FabricVerifyChecksum(),     c(),     h(),     FabricComputeChecksum(),     FabricDeparser() ) main;
