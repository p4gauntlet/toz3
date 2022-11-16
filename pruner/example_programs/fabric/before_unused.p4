#include <core.p4>
#define V1MODEL_VERSION 10
#include <v1model.p4>

const bit<4> GTPU_EXT_PSC_TYPE_DL = 4w10;
const bit<4> GTPU_EXT_PSC_TYPE_UL = 4w10;
typedef bit<3> fwd_type_t;
typedef bit<32> next_id_t;
typedef bit<20> mpls_label_t;
typedef bit<9> port_num_t;
typedef bit<48> mac_addr_t;
typedef bit<16> mcast_group_id_t;
typedef bit<12> vlan_id_t;
typedef bit<32> ipv4_addr_t;
typedef bit<16> l4_port_t;
typedef bit<4> slice_id_t;
typedef bit<2> tc_t;
typedef bit<6> slice_tc_t;
const slice_id_t DEFAULT_SLICE_ID = 4w10;
const tc_t DEFAULT_TC = 2w2;
typedef bit<2> direction_t;
typedef bit<8> spgw_interface_t;
typedef bit<1> pcc_gate_status_t;
typedef bit<32> sdf_rule_id_t;
typedef bit<32> pcc_rule_id_t;
typedef bit<32> far_id_t;
typedef bit<32> pdr_ctr_id_t;
typedef bit<32> teid_t;
typedef bit<6> qfi_t;
typedef bit<5> qid_t;
const spgw_interface_t SPGW_IFACE_UNKNOWN = 8w10;
const spgw_interface_t SPGW_IFACE_ACCESS = 8w10;
const spgw_interface_t SPGW_IFACE_CORE = 8w10;
const spgw_interface_t SPGW_IFACE_FROM_DBUF = 8w10;
typedef bit<2> port_type_t;
const port_type_t PORT_TYPE_UNKNOWN = 2w2;
const port_type_t PORT_TYPE_EDGE = 2w2;
const port_type_t PORT_TYPE_INFRA = 2w2;
const port_type_t PORT_TYPE_INTERNAL = 2w2;
const bit<16> ETHERTYPE_QINQ = 16w10;
const bit<16> ETHERTYPE_QINQ_NON_STD = 16w10;
const bit<16> ETHERTYPE_VLAN = 16w10;
const bit<16> ETHERTYPE_MPLS = 16w10;
const bit<16> ETHERTYPE_MPLS_MULTICAST = 16w10;
const bit<16> ETHERTYPE_IPV4 = 16w10;
const bit<16> ETHERTYPE_IPV6 = 16w10;
const bit<16> ETHERTYPE_ARP = 16w10;
const bit<16> ETHERTYPE_PPPOED = 16w10;
const bit<16> ETHERTYPE_PPPOES = 16w10;
const bit<16> PPPOE_PROTOCOL_IP4 = 16w10;
const bit<16> PPPOE_PROTOCOL_IP6 = 16w10;
const bit<16> PPPOE_PROTOCOL_MPLS = 16w10;
const bit<8> PROTO_ICMP = 8w10;
const bit<8> PROTO_TCP = 8w10;
const bit<8> PROTO_UDP = 8w10;
const bit<8> PROTO_ICMPV6 = 8w58;
const bit<4> IPV4_MIN_IHL = 4w10;
const fwd_type_t FWD_BRIDGING = 3w2;
const fwd_type_t FWD_MPLS = 3w2;
const fwd_type_t FWD_IPV4_UNICAST = 3w2;
const fwd_type_t FWD_IPV4_MULTICAST = 3w2;
const fwd_type_t FWD_IPV6_UNICAST = 3w2;
const fwd_type_t FWD_IPV6_MULTICAST = 3w2;
const fwd_type_t FWD_UNKNOWN = 3w2;
const vlan_id_t DEFAULT_VLAN_ID = 12w10;
const bit<8> DEFAULT_MPLS_TTL = 8w10;
const bit<8> DEFAULT_IPV4_TTL = 8w10;
const bit<6> INT_DSCP = 6w10;
const bit<8> INT_HEADER_LEN_WORDS = 8w10;
const bit<16> INT_HEADER_LEN_BYTES = 16w16;
const bit<8> CPU_MIRROR_SESSION_ID = 8w10;
const bit<32> REPORT_MIRROR_SESSION_ID = 32w10;
const bit<4> NPROTO_ETHERNET = 4w10;
const bit<4> NPROTO_TELEMETRY_DROP_HEADER = 4w10;
const bit<4> NPROTO_TELEMETRY_SWITCH_LOCAL_HEADER = 4w10;
const bit<6> HW_ID = 6w10;
const bit<8> REPORT_FIXED_HEADER_LEN = 8w12;
const bit<8> DROP_REPORT_HEADER_LEN = 8w10;
const bit<8> LOCAL_REPORT_HEADER_LEN = 8w10;
const bit<8> ETH_HEADER_LEN = 8w10;
const bit<8> IPV4_MIN_HEAD_LEN = 8w10;
const bit<8> UDP_HEADER_LEN = 8w10;
action nop() {
}
struct int_metadata_t {
    bool    source;
    bool    transit;
    bool    sink;
    bit<32> switch_id;
    bit<8>  new_words;
    bit<16> new_bytes;
    bit<32> ig_tstamp;
    bit<32> eg_tstamp;
}

header int_header_t {
    bit<2>  ver;
    bit<2>  rep;
    bit<1>  c;
    bit<1>  e;
    bit<5>  rsvd1;
    bit<5>  ins_cnt;
    bit<8>  max_hop_cnt;
    bit<8>  total_hop_cnt;
    bit<4>  instruction_mask_0003;
    bit<4>  instruction_mask_0407;
    bit<4>  instruction_mask_0811;
    bit<4>  instruction_mask_1215;
    bit<16> rsvd2;
}

header intl4_shim_t {
    bit<8> int_type;
    bit<8> rsvd1;
    bit<8> len_words;
    bit<8> rsvd2;
}

header intl4_tail_t {
    bit<8>  next_proto;
    bit<16> dest_port;
    bit<2>  padding;
    bit<6>  dscp;
}

@controller_header("packet_in") header packet_in_header_t {
    port_num_t ingress_port;
    bit<7>     _pad;
}

@controller_header("packet_out") header packet_out_header_t {
    port_num_t egress_port;
    bit<1>     do_forwarding;
    bit<6>     _pad;
}

header ethernet_t {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
}

header eth_type_t {
    bit<16> value;
}

header vlan_tag_t {
    bit<16>   eth_type;
    bit<3>    pri;
    bit<1>    cfi;
    vlan_id_t vlan_id;
}

header mpls_t {
    bit<20> label;
    bit<3>  tc;
    bit<1>  bos;
    bit<8>  ttl;
}

header pppoe_t {
    bit<4>  version;
    bit<4>  type_id;
    bit<8>  code;
    bit<16> session_id;
    bit<16> length;
    bit<16> protocol;
}

header ipv4_t {
    bit<4>  version;
    bit<4>  ihl;
    bit<6>  dscp;
    bit<2>  ecn;
    bit<16> total_len;
    bit<16> identification;
    bit<3>  flags;
    bit<13> frag_offset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdr_checksum;
    bit<32> src_addr;
    bit<32> dst_addr;
}

header ipv6_t {
    bit<4>   version;
    bit<8>   traffic_class;
    bit<20>  flow_label;
    bit<16>  payload_len;
    bit<8>   next_hdr;
    bit<8>   hop_limit;
    bit<128> src_addr;
    bit<128> dst_addr;
}

header tcp_t {
    bit<16> sport;
    bit<16> dport;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4>  data_offset;
    bit<3>  res;
    bit<3>  ecn;
    bit<6>  ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_t {
    bit<16> sport;
    bit<16> dport;
    bit<16> len;
    bit<16> checksum;
}

header icmp_t {
    bit<8>  icmp_type;
    bit<8>  icmp_code;
    bit<16> checksum;
    bit<16> identifier;
    bit<16> sequence_number;
    bit<64> timestamp;
}

header gtpu_t {
    bit<3>  version;
    bit<1>  pt;
    bit<1>  spare;
    bit<1>  ex_flag;
    bit<1>  seq_flag;
    bit<1>  npdu_flag;
    bit<8>  msgtype;
    bit<16> msglen;
    teid_t  teid;
}

header gtpu_options_t {
    bit<16> seq_num;
    bit<8>  n_pdu_num;
    bit<8>  next_ext;
}

header gtpu_ext_psc_t {
    bit<8> len;
    bit<4> type;
    bit<4> spare0;
    bit<1> ppp;
    bit<1> rqi;
    qfi_t  qfi;
    bit<8> next_ext;
}

struct lookup_metadata_t {
    bool      is_ipv4;
    bit<32>   ipv4_src;
    bit<32>   ipv4_dst;
    bit<8>    ip_proto;
    l4_port_t l4_sport;
    l4_port_t l4_dport;
    bit<8>    icmp_type;
    bit<8>    icmp_code;
}

struct fabric_metadata_t {
    lookup_metadata_t lkp;
    bit<16>           ip_eth_type;
    vlan_id_t         vlan_id;
    bit<3>            vlan_pri;
    bit<1>            vlan_cfi;
    mpls_label_t      mpls_label;
    bit<8>            mpls_ttl;
    bool              skip_forwarding;
    bool              skip_next;
    fwd_type_t        fwd_type;
    next_id_t         next_id;
    bool              is_multicast;
    bool              is_controller_packet_out;
    bit<8>            ip_proto;
    bit<16>           l4_sport;
    bit<16>           l4_dport;
    bit<32>           ipv4_src_addr;
    bit<32>           ipv4_dst_addr;
    slice_id_t        slice_id;
    bit<2>            packet_color;
    tc_t              tc;
    bit<6>            dscp;
    port_type_t       port_type;
}

struct parsed_headers_t {
    ethernet_t          ethernet;
    vlan_tag_t          vlan_tag;
    vlan_tag_t          inner_vlan_tag;
    eth_type_t          eth_type;
    mpls_t              mpls;
    gtpu_t              gtpu;
    gtpu_options_t      gtpu_options;
    gtpu_ext_psc_t      gtpu_ext_psc;
    ipv4_t              inner_ipv4;
    udp_t               inner_udp;
    tcp_t               inner_tcp;
    icmp_t              inner_icmp;
    ipv4_t              ipv4;
    tcp_t               tcp;
    udp_t               udp;
    icmp_t              icmp;
    packet_out_header_t packet_out;
    packet_in_header_t  packet_in;
}

control Filtering(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    direct_counter(CounterType.packets_and_bytes) ingress_port_vlan_counter;
    action deny() {
    }
    action permit(port_type_t port_type) {
    }
    action permit_with_internal_vlan(vlan_id_t vlan_id, port_type_t port_type) {
    }
    table ingress_port_vlan {
        key = {
            standard_metadata.ingress_port: exact @name("ig_port") ;
            hdr.vlan_tag.isValid()        : exact @name("vlan_is_valid") ;
            hdr.vlan_tag.vlan_id          : ternary @name("vlan_id") ;
        }
        actions = {
            deny();
            permit();
            permit_with_internal_vlan();
        }
        const default_action = deny();
        counters = ingress_port_vlan_counter;
        size = 1024;
    }
    direct_counter(CounterType.packets_and_bytes) fwd_classifier_counter;
    action set_forwarding_type(fwd_type_t fwd_type) {
    }
    table fwd_classifier {
        key = {
            standard_metadata.ingress_port: exact @name("ig_port") ;
            hdr.ethernet.dst_addr         : ternary @name("eth_dst") ;
            hdr.eth_type.value            : ternary @name("eth_type") ;
            fabric_metadata.ip_eth_type   : exact @name("ip_eth_type") ;
        }
        actions = {
            set_forwarding_type();
        }
        const default_action = set_forwarding_type(3w2);
        counters = fwd_classifier_counter;
        size = 1024;
    }
    apply {
    }
}

control Forwarding(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    @hidden action set_next_id(next_id_t next_id) {
    }
    direct_counter(CounterType.packets_and_bytes) bridging_counter;
    action set_next_id_bridging(next_id_t next_id) {
    }
    table bridging {
        key = {
            fabric_metadata.vlan_id: exact @name("vlan_id") ;
            hdr.ethernet.dst_addr  : ternary @name("eth_dst") ;
        }
        actions = {
            set_next_id_bridging();
            @defaultonly nop();
        }
        const default_action = nop();
        counters = bridging_counter;
        size = 1024;
    }
    direct_counter(CounterType.packets_and_bytes) mpls_counter;
    action pop_mpls_and_next(next_id_t next_id) {
    }
    table mpls {
        key = {
            fabric_metadata.mpls_label: exact @name("mpls_label") ;
        }
        actions = {
            pop_mpls_and_next();
            @defaultonly nop();
        }
        const default_action = nop();
        counters = mpls_counter;
        size = 1024;
    }
    action set_next_id_routing_v4(next_id_t next_id) {
    }
    action nop_routing_v4() {
    }
    table routing_v4 {
        key = {
            fabric_metadata.ipv4_dst_addr: lpm @name("ipv4_dst") ;
        }
        actions = {
            set_next_id_routing_v4();
            nop_routing_v4();
            @defaultonly nop();
        }
        default_action = nop();
        size = 1024;
    }
    apply {
    }
}

control PreNext(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata) {
    direct_counter(CounterType.packets_and_bytes) next_mpls_counter;
    action set_mpls_label(mpls_label_t label) {
    }
    table next_mpls {
        key = {
            fabric_metadata.next_id: exact @name("next_id") ;
        }
        actions = {
            set_mpls_label();
            @defaultonly nop();
        }
        const default_action = nop();
        counters = next_mpls_counter;
        size = 1024;
    }
    direct_counter(CounterType.packets_and_bytes) next_vlan_counter;
    action set_vlan(vlan_id_t vlan_id) {
    }
    table next_vlan {
        key = {
            fabric_metadata.next_id: exact @name("next_id") ;
        }
        actions = {
            set_vlan();
            @defaultonly nop();
        }
        const default_action = nop();
        counters = next_vlan_counter;
        size = 1024;
    }
    apply {
    }
}

control Acl(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_md, inout standard_metadata_t standard_metadata) {
    direct_counter(CounterType.packets_and_bytes) acl_counter;
    action set_next_id_acl(next_id_t next_id) {
    }
    action punt_to_cpu() {
    }
    action set_clone_session_id(bit<32> clone_id) {
    }
    action drop() {
    }
    action nop_acl() {
    }
    table acl {
        key = {
            standard_metadata.ingress_port: ternary @name("ig_port") ;
            hdr.ethernet.dst_addr         : ternary @name("eth_dst") ;
            hdr.ethernet.src_addr         : ternary @name("eth_src") ;
            hdr.vlan_tag.vlan_id          : ternary @name("vlan_id") ;
            hdr.eth_type.value            : ternary @name("eth_type") ;
            fabric_md.lkp.ipv4_src        : ternary @name("ipv4_src") ;
            fabric_md.lkp.ipv4_dst        : ternary @name("ipv4_dst") ;
            fabric_md.lkp.ip_proto        : ternary @name("ip_proto") ;
            hdr.icmp.icmp_type            : ternary @name("icmp_type") ;
            hdr.icmp.icmp_code            : ternary @name("icmp_code") ;
            fabric_md.lkp.l4_sport        : ternary @name("l4_sport") ;
            fabric_md.lkp.l4_dport        : ternary @name("l4_dport") ;
            fabric_md.port_type           : ternary @name("port_type") ;
        }
        actions = {
            set_next_id_acl();
            punt_to_cpu();
            set_clone_session_id();
            drop();
            nop_acl();
        }
        const default_action = nop_acl();
        size = 1024;
        counters = acl_counter;
    }
    apply {
    }
}

control Next(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    @hidden action output(port_num_t port_num) {
    }
    @hidden action rewrite_smac(mac_addr_t smac) {
    }
    @hidden action rewrite_dmac(mac_addr_t dmac) {
    }
    @hidden action routing(port_num_t port_num, mac_addr_t smac, mac_addr_t dmac) {
    }
    direct_counter(CounterType.packets_and_bytes) xconnect_counter;
    action output_xconnect(port_num_t port_num) {
    }
    action set_next_id_xconnect(next_id_t next_id) {
    }
    table xconnect {
        key = {
            standard_metadata.ingress_port: exact @name("ig_port") ;
            fabric_metadata.next_id       : exact @name("next_id") ;
        }
        actions = {
            output_xconnect();
            set_next_id_xconnect();
            @defaultonly nop();
        }
        counters = xconnect_counter;
        const default_action = nop();
        size = 1024;
    }
    @max_group_size(16) action_selector(HashAlgorithm.crc16, 32w10, 32w10) hashed_selector;
    direct_counter(CounterType.packets_and_bytes) hashed_counter;
    action output_hashed(port_num_t port_num) {
    }
    action routing_hashed(port_num_t port_num, mac_addr_t smac, mac_addr_t dmac) {
    }
    table hashed {
        key = {
            fabric_metadata.next_id      : exact @name("next_id") ;
            fabric_metadata.ipv4_src_addr: selector;
            fabric_metadata.ipv4_dst_addr: selector;
            fabric_metadata.ip_proto     : selector;
            fabric_metadata.l4_sport     : selector;
            fabric_metadata.l4_dport     : selector;
        }
        actions = {
            output_hashed();
            routing_hashed();
            @defaultonly nop();
        }
        implementation = hashed_selector;
        counters = hashed_counter;
        const default_action = nop();
        size = 1024;
    }
    direct_counter(CounterType.packets_and_bytes) multicast_counter;
    action set_mcast_group_id(mcast_group_id_t group_id) {
    }
    table multicast {
        key = {
            fabric_metadata.next_id: exact @name("next_id") ;
        }
        actions = {
            set_mcast_group_id();
            @defaultonly nop();
        }
        counters = multicast_counter;
        const default_action = nop();
        size = 1024;
    }
    apply {
    }
}

control EgressNextControl(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    @hidden action pop_mpls_if_present() {
    }
    @hidden action set_mpls() {
    }
    @hidden action push_outer_vlan() {
    }
    direct_counter(CounterType.packets_and_bytes) egress_vlan_counter;
    action push_vlan() {
    }
    action pop_vlan() {
    }
    action drop() {
    }
    table egress_vlan {
        key = {
            fabric_metadata.vlan_id      : exact @name("vlan_id") ;
            standard_metadata.egress_port: exact @name("eg_port") ;
        }
        actions = {
            push_vlan();
            pop_vlan();
            @defaultonly drop();
        }
        const default_action = drop();
        counters = egress_vlan_counter;
        size = 1024;
    }
    apply {
    }
}

control PacketIoIngress(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    apply {
    }
}

control PacketIoEgress(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    apply {
    }
}

control LookupMdInit(in parsed_headers_t hdr, out lookup_metadata_t lkp_md) {
    apply {
    }
}

control IngressSliceTcClassifier(in parsed_headers_t hdr, inout fabric_metadata_t fabric_md, in standard_metadata_t standard_metadata) {
    direct_counter(CounterType.packets) classifier_stats;
    action set_slice_id_tc(slice_id_t slice_id, tc_t tc) {
    }
    action trust_dscp() {
    }
    table classifier {
        key = {
            9w10                  : ternary @name("ig_port") ;
            fabric_md.lkp.ipv4_src: ternary @name("ipv4_src") ;
            fabric_md.lkp.ipv4_dst: ternary @name("ipv4_dst") ;
            fabric_md.lkp.ip_proto: ternary @name("ip_proto") ;
            fabric_md.lkp.l4_sport: ternary @name("l4_sport") ;
            fabric_md.lkp.l4_dport: ternary @name("l4_dport") ;
        }
        actions = {
            set_slice_id_tc();
            trust_dscp();
        }
        const default_action = set_slice_id_tc(4w10, DEFAULT_TC);
        counters = classifier_stats;
        size = 512;
    }
    apply {
    }
}

control IngressQos(inout fabric_metadata_t fabric_md, inout standard_metadata_t standard_metadata) {
    meter(32w10, MeterType.bytes) slice_tc_meter;
    direct_counter(CounterType.packets) queues_stats;
    action set_queue(qid_t qid) {
    }
    action meter_drop() {
    }
    table queues {
        key = {
            fabric_md.slice_id    : exact @name("slice_id") ;
            fabric_md.tc          : exact @name("tc") ;
            fabric_md.packet_color: ternary @name("color") ;
        }
        actions = {
            set_queue();
            meter_drop();
        }
        const default_action = set_queue(5w10);
        counters = queues_stats;
        size = 1;
    }
    slice_tc_t slice_tc = 6w10;
    apply {
    }
}

control EgressDscpRewriter(inout parsed_headers_t hdr, in fabric_metadata_t fabric_md, in standard_metadata_t standard_metadata) {
    bit<6> tmp_dscp = fabric_md.dscp;
    action rewrite() {
    }
    action clear() {
    }
    table rewriter {
        key = {
            9w10: exact @name("eg_port") ;
        }
        actions = {
            rewrite();
            clear();
            @defaultonly nop();
        }
        const default_action = nop();
        size = 512;
    }
    apply {
    }
}

control FabricComputeChecksum(inout parsed_headers_t hdr, inout fabric_metadata_t meta) {
    apply {
    }
}

control FabricVerifyChecksum(inout parsed_headers_t hdr, inout fabric_metadata_t meta) {
    apply {
    }
}

parser FabricParser(packet_in packet, out parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    bit<6> last_ipv4_dscp = 6w10;
    state start {
        transition check_packet_out;
    }
    state check_packet_out {
        packet_out_header_t tmp = packet.lookahead<packet_out_header_t>();
        transition parse_packet_out_and_accept;
    }
    state parse_packet_out_and_accept {
        transition accept;
    }
    state strip_packet_out {
        transition parse_ethernet;
    }
    state parse_ethernet {
        transition parse_vlan_tag;
    }
    state parse_vlan_tag {
        transition parse_inner_vlan_tag;
    }
    state parse_inner_vlan_tag {
        transition parse_eth_type;
    }
    state parse_eth_type {
        transition parse_mpls;
    }
    state parse_mpls {
        transition parse_ipv4;
    }
    state parse_ipv4 {
        transition parse_tcp;
    }
    state parse_tcp {
        transition accept;
    }
    state parse_udp {
        gtpu_t gtpu = packet.lookahead<gtpu_t>();
        transition parse_gtpu;
    }
    state parse_icmp {
        transition accept;
    }
    state parse_gtpu {
        transition parse_inner_ipv4;
    }
    state parse_gtpu_options {
        bit<8> gtpu_ext_len = packet.lookahead<bit<8>>();
        transition parse_gtpu_ext_psc;
    }
    state parse_gtpu_ext_psc {
        transition parse_inner_ipv4;
    }
    state parse_inner_ipv4 {
        transition parse_inner_tcp;
    }
    state parse_inner_udp {
        transition accept;
    }
    state parse_inner_tcp {
        transition accept;
    }
    state parse_inner_icmp {
        transition accept;
    }
}

control FabricDeparser(packet_out packet, in parsed_headers_t hdr) {
    apply {
    }
}

bit<16> test_func(inout ipv4_t istd) {
    return istd.total_len;
}
control FabricIngress(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    LookupMdInit() lkp_md_init;
    PacketIoIngress() pkt_io_ingress;
    Filtering() filtering;
    Forwarding() forwarding;
    PreNext() pre_next;
    Acl() acl;
    Next() next;
    IngressSliceTcClassifier() slice_tc_classifier;
    IngressQos() qos;
    table clb_pinned_flows {
        key = {
            test_func(hdr.inner_ipv4): exact @name("ipv4_addr_1") ;
        }
        actions = {
            NoAction();
        }
        const default_action = NoAction();
    }
    apply {
    }
}

control FabricEgress(inout parsed_headers_t hdr, inout fabric_metadata_t fabric_metadata, inout standard_metadata_t standard_metadata) {
    PacketIoEgress() pkt_io_egress;
    EgressNextControl() egress_next;
    EgressDscpRewriter() dscp_rewriter;
    apply {
    }
}

V1Switch(FabricParser(), FabricVerifyChecksum(), FabricIngress(), FabricEgress(), FabricComputeChecksum(), FabricDeparser()) main;

