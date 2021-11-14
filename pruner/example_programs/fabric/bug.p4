#include <pna.p4>

typedef bit<48>  EthernetAddress;

header ethernet_t {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16>         etherType;
    PassNumber_t pass;
    PNA_Direction_t direction;
}

header ipv4_t {
    bit<4>  version;
    bit<4>  ihl;
    bit<8>  diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3>  flags;
    bit<13> fragOffset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdrChecksum;
    bit<32> srcAddr;
    bit<32> dstAddr;
}

struct headers_t {
    ethernet_t ethernet;
    ipv4_t ipv4;
}

struct main_metadata_t {
   PortId_t dst_port;
}

bool RxPkt (in ethernet_t istd) {
    return (istd.etherType == 10);
}

bool TxPkt (in ethernet_t istd) {
    return (istd.etherType == 2);
}

bool pass_1st (in ethernet_t istd) {
    return (istd.pass == (PassNumber_t)0);
}

bool pass_2nd (in ethernet_t istd) {
    return (istd.pass == (PassNumber_t)1);
}


control PreControlImpl(
    in    headers_t  hdr,
    inout main_metadata_t meta,
    in    pna_pre_input_metadata_t  istd,
    inout pna_pre_output_metadata_t ostd)
{
    apply {
    }
}

parser MainParserImpl(
    packet_in pkt,
    out   headers_t       hdr,
    inout main_metadata_t main_meta,
    in    pna_main_parser_input_metadata_t istd)
{
    state start {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            0x0800: parse_ipv4;
            default: accept;
        }
    }
    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition accept;
    }
}

control MainControlImpl(
    inout headers_t  hdr,
    inout main_metadata_t meta,
    in    pna_main_input_metadata_t  istd,
    inout pna_main_output_metadata_t ostd)
{
    table clb_pinned_flows {
        key = {
            // other key fields also possible, e.g. VRF
            SelectByDirection(hdr.ethernet.direction, hdr.ipv4.srcAddr,
                                              hdr.ipv4.dstAddr):
                exact @name("ipv4_addr_0");
            SelectByDirection(hdr.ethernet.direction, hdr.ipv4.dstAddr,
                                              hdr.ipv4.srcAddr):
                exact @name("ipv4_addr_1");
            hdr.ipv4.protocol : exact;
        }
        actions = {
            NoAction;
        }
        const default_action = NoAction;
    }

    apply {
        if (TxPkt(hdr.ethernet) && pass_1st(hdr.ethernet)) {
            clb_pinned_flows.apply();
        } else if (TxPkt(hdr.ethernet) && pass_2nd(hdr.ethernet)) {
            clb_pinned_flows.apply();
        }
    }
}


control MainDeparserImpl(
    packet_out pkt,
    in    headers_t hdr,                // from main control
    in    main_metadata_t user_meta,    // from main control
    in    pna_main_output_metadata_t ostd)
{
    apply {
        pkt.emit(hdr.ethernet);
        pkt.emit(hdr.ipv4);
    }
}

PNA_NIC(
    MainParserImpl(),
    PreControlImpl(),
    MainControlImpl(),
    MainDeparserImpl()
    ) main;
