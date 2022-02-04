from p4z3 import *



def p4_program(prog_state):
    prog_state.declare_global(
        Enum( "error", ["NoError", "PacketTooShort", "NoMatch", "StackOutOfBounds", "HeaderTooShort", "ParserTimeout", "ParserInvalidArgument", "CounterRange", "Timeout", "PhvOwner", "MultiWrite", "IbufOverflow", "IbufUnderflow", ])
    )
    prog_state.declare_global(
        P4Extern("packet_in", type_params=[], methods=[P4Declaration("extract", P4Method("extract", type_params=(None, [
            "T",]), params=[
            P4Parameter("out", "hdr", "T", None),])), P4Declaration("extract", P4Method("extract", type_params=(None, [
            "T",]), params=[
            P4Parameter("out", "variableSizeHeader", "T", None),
            P4Parameter("in", "variableFieldSizeInBits", z3.BitVecSort(32), None),])), P4Declaration("lookahead", P4Method("lookahead", type_params=("T", [
            "T",]), params=[])), P4Declaration("advance", P4Method("advance", type_params=(None, []), params=[
            P4Parameter("in", "sizeInBits", z3.BitVecSort(32), None),])), P4Declaration("length", P4Method("length", type_params=(z3.BitVecSort(32), []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("packet_out", type_params=[], methods=[P4Declaration("emit", P4Method("emit", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "hdr", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Declaration("verify", P4Method("verify", type_params=(None, []), params=[
            P4Parameter("in", "check", z3.BoolSort(), None),
            P4Parameter("in", "toSignal", "error", None),]))
    )
    prog_state.declare_global(
        P4Declaration("match_kind", ["exact", "ternary", "lpm", ])
    )
    prog_state.declare_global(
        TypeDeclaration("MirrorId_t", z3.BitVecSort(10))
    )
    prog_state.declare_global(
        Enum( "MeterType_t", ["PACKETS", "BYTES", ])
    )
    prog_state.declare_global(
        SerEnum( "MeterColor_t", [("GREEN", z3.BitVecVal(0, 8)), ("YELLOW", z3.BitVecVal(1, 8)), ("RED", z3.BitVecVal(3, 8)), ], z3.BitVecSort(8))
    )
    prog_state.declare_global(
        Enum( "CounterType_t", ["PACKETS", "BYTES", "PACKETS_AND_BYTES", ])
    )
    prog_state.declare_global(
        Enum( "SelectorMode_t", ["FAIR", "RESILIENT", ])
    )
    prog_state.declare_global(
        Enum( "HashAlgorithm_t", ["IDENTITY", "RANDOM", "CRC8", "CRC16", "CRC32", "CRC64", "CUSTOM", ])
    )
    prog_state.declare_global(
        P4Declaration("match_kind", ["range", "selector", "atcam_partition_index", ])
    )
    prog_state.declare_global(
        HeaderType("ingress_intrinsic_metadata_t", prog_state, fields=[("ingress_port", z3.BitVecSort(9)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_intrinsic_metadata_for_tm_t", prog_state, fields=[("ucast_egress_port", z3.BitVecSort(9)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_intrinsic_metadata_from_parser_t", prog_state, fields=[("parser_err", z3.BitVecSort(16)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_intrinsic_metadata_for_deparser_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        HeaderType("egress_intrinsic_metadata_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("egress_intrinsic_metadata_from_parser_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("egress_intrinsic_metadata_for_deparser_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("egress_intrinsic_metadata_for_output_port_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        P4Extern("Checksum", type_params=[], methods=[P4Declaration("Checksum", P4Method("Checksum", type_params=(None, []), params=[])), P4Declaration("add", P4Method("add", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "data", "T", None),])), P4Declaration("subtract", P4Method("subtract", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "data", "T", None),])), P4Declaration("verify", P4Method("verify", type_params=(z3.BoolSort(), []), params=[])), P4Declaration("get", P4Method("get", type_params=(z3.BitVecSort(16), []), params=[])), P4Declaration("update", P4Method("update", type_params=(z3.BitVecSort(16), [
            "T",]), params=[
            P4Parameter("in", "data", "T", None),
            P4Parameter("in", "zeros_as_ones", z3.BoolSort(), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("ParserCounter", type_params=[], methods=[P4Declaration("ParserCounter", P4Method("ParserCounter", type_params=(None, []), params=[])), P4Declaration("set", P4Method("set", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "value", "T", None),])), P4Declaration("set", P4Method("set", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "field", "T", None),
            P4Parameter("in", "max", z3.BitVecSort(8), None),
            P4Parameter("in", "rotate", z3.BitVecSort(8), None),
            P4Parameter("in", "mask", z3.BitVecSort(3), None),
            P4Parameter("in", "add", z3.BitVecSort(8), None),])), P4Declaration("is_zero", P4Method("is_zero", type_params=(z3.BoolSort(), []), params=[])), P4Declaration("is_negative", P4Method("is_negative", type_params=(z3.BoolSort(), []), params=[])), P4Declaration("increment", P4Method("increment", type_params=(None, []), params=[
            P4Parameter("in", "value", z3.BitVecSort(8), None),])), P4Declaration("decrement", P4Method("decrement", type_params=(None, []), params=[
            P4Parameter("in", "value", z3.BitVecSort(8), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("ParserPriority", type_params=[], methods=[P4Declaration("ParserPriority", P4Method("ParserPriority", type_params=(None, []), params=[])), P4Declaration("set", P4Method("set", type_params=(None, []), params=[
            P4Parameter("in", "prio", z3.BitVecSort(3), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("CRCPolynomial", type_params=[
            "T",], methods=[P4Declaration("CRCPolynomial", P4Method("CRCPolynomial", type_params=(None, []), params=[
            P4Parameter("none", "coeff", "T", None),
            P4Parameter("none", "reversed", z3.BoolSort(), None),
            P4Parameter("none", "msb", z3.BoolSort(), None),
            P4Parameter("none", "extended", z3.BoolSort(), None),
            P4Parameter("none", "init", "T", None),
            P4Parameter("none", "xor", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Hash", type_params=[
            "W",], methods=[P4Declaration("Hash", P4Method("Hash", type_params=(None, []), params=[
            P4Parameter("none", "algo", "HashAlgorithm_t", None),])), P4Declaration("Hash", P4Method("Hash", type_params=(None, []), params=[
            P4Parameter("none", "algo", "HashAlgorithm_t", None),
            P4Parameter("none", "poly", TypeSpecializer("CRCPolynomial", None, ), None),])), P4Declaration("get", P4Method("get", type_params=("W", [
            "D",]), params=[
            P4Parameter("in", "data", "D", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Random", type_params=[
            "W",], methods=[P4Declaration("Random", P4Method("Random", type_params=(None, []), params=[])), P4Declaration("get", P4Method("get", type_params=("W", []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("Counter", type_params=[
            "W",
            "I",], methods=[P4Declaration("Counter", P4Method("Counter", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "type", "CounterType_t", None),])), P4Declaration("count", P4Method("count", type_params=(None, []), params=[
            P4Parameter("in", "index", "I", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("DirectCounter", type_params=[
            "W",], methods=[P4Declaration("DirectCounter", P4Method("DirectCounter", type_params=(None, []), params=[
            P4Parameter("none", "type", "CounterType_t", None),])), P4Declaration("count", P4Method("count", type_params=(None, []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("Meter", type_params=[
            "I",], methods=[P4Declaration("Meter", P4Method("Meter", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "type", "MeterType_t", None),])), P4Declaration("Meter", P4Method("Meter", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "type", "MeterType_t", None),
            P4Parameter("none", "red", z3.BitVecSort(8), None),
            P4Parameter("none", "yellow", z3.BitVecSort(8), None),
            P4Parameter("none", "green", z3.BitVecSort(8), None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[
            P4Parameter("in", "index", "I", None),
            P4Parameter("in", "color", "MeterColor_t", None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[
            P4Parameter("in", "index", "I", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("DirectMeter", type_params=[], methods=[P4Declaration("DirectMeter", P4Method("DirectMeter", type_params=(None, []), params=[
            P4Parameter("none", "type", "MeterType_t", None),])), P4Declaration("DirectMeter", P4Method("DirectMeter", type_params=(None, []), params=[
            P4Parameter("none", "type", "MeterType_t", None),
            P4Parameter("none", "red", z3.BitVecSort(8), None),
            P4Parameter("none", "yellow", z3.BitVecSort(8), None),
            P4Parameter("none", "green", z3.BitVecSort(8), None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[
            P4Parameter("in", "color", "MeterColor_t", None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("Lpf", type_params=[
            "T",
            "I",], methods=[P4Declaration("Lpf", P4Method("Lpf", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),])), P4Declaration("execute", P4Method("execute", type_params=("T", []), params=[
            P4Parameter("in", "val", "T", None),
            P4Parameter("in", "index", "I", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("DirectLpf", type_params=[
            "T",], methods=[P4Declaration("DirectLpf", P4Method("DirectLpf", type_params=(None, []), params=[])), P4Declaration("execute", P4Method("execute", type_params=("T", []), params=[
            P4Parameter("in", "val", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Wred", type_params=[
            "T",
            "I",], methods=[P4Declaration("Wred", P4Method("Wred", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "drop_value", z3.BitVecSort(8), None),
            P4Parameter("none", "no_drop_value", z3.BitVecSort(8), None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[
            P4Parameter("in", "val", "T", None),
            P4Parameter("in", "index", "I", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("DirectWred", type_params=[
            "T",], methods=[P4Declaration("DirectWred", P4Method("DirectWred", type_params=(None, []), params=[
            P4Parameter("none", "drop_value", z3.BitVecSort(8), None),
            P4Parameter("none", "no_drop_value", z3.BitVecSort(8), None),])), P4Declaration("execute", P4Method("execute", type_params=(z3.BitVecSort(8), []), params=[
            P4Parameter("in", "val", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Register", type_params=[
            "T",
            "I",], methods=[P4Declaration("Register", P4Method("Register", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),])), P4Declaration("Register", P4Method("Register", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "initial_value", "T", None),])), P4Declaration("read", P4Method("read", type_params=("T", []), params=[
            P4Parameter("in", "index", "I", None),])), P4Declaration("write", P4Method("write", type_params=(None, []), params=[
            P4Parameter("in", "index", "I", None),
            P4Parameter("in", "value", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("DirectRegister", type_params=[
            "T",], methods=[P4Declaration("DirectRegister", P4Method("DirectRegister", type_params=(None, []), params=[])), P4Declaration("DirectRegister", P4Method("DirectRegister", type_params=(None, []), params=[
            P4Parameter("none", "initial_value", "T", None),])), P4Declaration("read", P4Method("read", type_params=("T", []), params=[])), P4Declaration("write", P4Method("write", type_params=(None, []), params=[
            P4Parameter("in", "value", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("RegisterParam", type_params=[
            "T",], methods=[P4Declaration("RegisterParam", P4Method("RegisterParam", type_params=(None, []), params=[
            P4Parameter("none", "initial_value", "T", None),])), P4Declaration("read", P4Method("read", type_params=("T", []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("ActionProfile", type_params=[], methods=[P4Declaration("ActionProfile", P4Method("ActionProfile", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("ActionSelector", type_params=[], methods=[P4Declaration("ActionSelector", P4Method("ActionSelector", type_params=(None, []), params=[
            P4Parameter("none", "action_profile", "ActionProfile", None),
            P4Parameter("none", "hash", TypeSpecializer("Hash", None, ), None),
            P4Parameter("none", "mode", "SelectorMode_t", None),
            P4Parameter("none", "max_group_size", z3.BitVecSort(32), None),
            P4Parameter("none", "num_groups", z3.BitVecSort(32), None),])), P4Declaration("ActionSelector", P4Method("ActionSelector", type_params=(None, []), params=[
            P4Parameter("none", "action_profile", "ActionProfile", None),
            P4Parameter("none", "hash", TypeSpecializer("Hash", None, ), None),
            P4Parameter("none", "mode", "SelectorMode_t", None),
            P4Parameter("none", "reg", TypeSpecializer("Register", z3.BitVecSort(1), None, ), None),
            P4Parameter("none", "max_group_size", z3.BitVecSort(32), None),
            P4Parameter("none", "num_groups", z3.BitVecSort(32), None),])), P4Declaration("ActionSelector", P4Method("ActionSelector", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "hash", TypeSpecializer("Hash", None, ), None),
            P4Parameter("none", "mode", "SelectorMode_t", None),])), P4Declaration("ActionSelector", P4Method("ActionSelector", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "hash", TypeSpecializer("Hash", None, ), None),
            P4Parameter("none", "mode", "SelectorMode_t", None),
            P4Parameter("none", "reg", TypeSpecializer("Register", z3.BitVecSort(1), None, ), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Mirror", type_params=[], methods=[P4Declaration("Mirror", P4Method("Mirror", type_params=(None, []), params=[])), P4Declaration("emit", P4Method("emit", type_params=(None, []), params=[
            P4Parameter("in", "session_id", "MirrorId_t", None),])), P4Declaration("emit", P4Method("emit", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "session_id", "MirrorId_t", None),
            P4Parameter("in", "hdr", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Resubmit", type_params=[], methods=[P4Declaration("Resubmit", P4Method("Resubmit", type_params=(None, []), params=[])), P4Declaration("emit", P4Method("emit", type_params=(None, []), params=[])), P4Declaration("emit", P4Method("emit", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "hdr", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Digest", type_params=[
            "T",], methods=[P4Declaration("Digest", P4Method("Digest", type_params=(None, []), params=[])), P4Declaration("pack", P4Method("pack", type_params=(None, []), params=[
            P4Parameter("in", "data", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Atcam", type_params=[], methods=[P4Declaration("Atcam", P4Method("Atcam", type_params=(None, []), params=[
            P4Parameter("none", "number_partitions", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("Alpm", type_params=[], methods=[P4Declaration("Alpm", P4Method("Alpm", type_params=(None, []), params=[
            P4Parameter("none", "number_partitions", z3.BitVecSort(32), None),
            P4Parameter("none", "subtrees_per_partition", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        ControlDeclaration(P4ParserType("IngressParserT", params=[
            P4Parameter("none", "pkt", "packet_in", None),
            P4Parameter("out", "hdr", "H", None),
            P4Parameter("out", "ig_md", "M", None),
            P4Parameter("out", "ig_intr_md", "ingress_intrinsic_metadata_t", None),
            P4Parameter("out", "ig_intr_md_for_tm", "ingress_intrinsic_metadata_for_tm_t", None),
            P4Parameter("out", "ig_intr_md_from_prsr", "ingress_intrinsic_metadata_from_parser_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ParserType("EgressParserT", params=[
            P4Parameter("none", "pkt", "packet_in", None),
            P4Parameter("out", "hdr", "H", None),
            P4Parameter("out", "eg_md", "M", None),
            P4Parameter("out", "eg_intr_md", "egress_intrinsic_metadata_t", None),
            P4Parameter("out", "eg_intr_md_from_prsr", "egress_intrinsic_metadata_from_parser_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("IngressT", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "ig_md", "M", None),
            P4Parameter("in", "ig_intr_md", "ingress_intrinsic_metadata_t", None),
            P4Parameter("in", "ig_intr_md_from_prsr", "ingress_intrinsic_metadata_from_parser_t", None),
            P4Parameter("inout", "ig_intr_md_for_dprsr", "ingress_intrinsic_metadata_for_deparser_t", None),
            P4Parameter("inout", "ig_intr_md_for_tm", "ingress_intrinsic_metadata_for_tm_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("EgressT", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "eg_md", "M", None),
            P4Parameter("in", "eg_intr_md", "egress_intrinsic_metadata_t", None),
            P4Parameter("in", "eg_intr_md_from_prsr", "egress_intrinsic_metadata_from_parser_t", None),
            P4Parameter("inout", "eg_intr_md_for_dprsr", "egress_intrinsic_metadata_for_deparser_t", None),
            P4Parameter("inout", "eg_intr_md_for_oport", "egress_intrinsic_metadata_for_output_port_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("IngressDeparserT", params=[
            P4Parameter("none", "pkt", "packet_out", None),
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("in", "metadata", "M", None),
            P4Parameter("in", "ig_intr_md_for_dprsr", "ingress_intrinsic_metadata_for_deparser_t", None),
            P4Parameter("in", "ig_intr_md", "ingress_intrinsic_metadata_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("EgressDeparserT", params=[
            P4Parameter("none", "pkt", "packet_out", None),
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("in", "metadata", "M", None),
            P4Parameter("in", "eg_intr_md_for_dprsr", "egress_intrinsic_metadata_for_deparser_t", None),
            P4Parameter("in", "eg_intr_md", "egress_intrinsic_metadata_t", None),
            P4Parameter("in", "eg_intr_md_from_prsr", "egress_intrinsic_metadata_from_parser_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("Pipeline", params=[
            P4Parameter("none", "ingress_parser", TypeSpecializer("IngressParserT", "IH", "IM", ), None),
            P4Parameter("none", "ingress", TypeSpecializer("IngressT", "IH", "IM", ), None),
            P4Parameter("none", "ingress_deparser", TypeSpecializer("IngressDeparserT", "IH", "IM", ), None),
            P4Parameter("none", "egress_parser", TypeSpecializer("EgressParserT", "EH", "EM", ), None),
            P4Parameter("none", "egress", TypeSpecializer("EgressT", "EH", "EM", ), None),
            P4Parameter("none", "egress_deparser", TypeSpecializer("EgressDeparserT", "EH", "EM", ), None),],type_params=[
            "IH",
            "IM",
            "EH",
            "EM",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("Switch", params=[
            P4Parameter("none", "pipe0", TypeSpecializer("Pipeline", "IH0", "IM0", "EH0", "EM0", ), None),
            P4Parameter("none", "pipe1", TypeSpecializer("Pipeline", "IH1", "IM1", "EH1", "EM1", ), None),
            P4Parameter("none", "pipe2", TypeSpecializer("Pipeline", "IH2", "IM2", "EH2", "EM2", ), None),
            P4Parameter("none", "pipe3", TypeSpecializer("Pipeline", "IH3", "IM3", "EH3", "EM3", ), None),],type_params=[
            "IH0",
            "IM0",
            "EH0",
            "EM0",
            "IH1",
            "IM1",
            "EH1",
            "EM1",
            "IH2",
            "IM2",
            "EH2",
            "EM2",
            "IH3",
            "IM3",
            "EH3",
            "EM3",]))
    )
    prog_state.declare_global(
        HeaderType("ethernet_h", prog_state, fields=[("dst_addr", z3.BitVecSort(48)), ("src_addr", z3.BitVecSort(48)), ("eth_type", z3.BitVecSort(16)), ], type_params=[])
    )
    prog_state.declare_global(
        TypeDeclaration("nexthop_t", z3.BitVecSort(16))
    )
    prog_state.declare_global(
        TypeDeclaration("srv6_sid_t", z3.BitVecSort(128))
    )
    prog_state.declare_global(
        StructType("srv6_metadata_t", prog_state, fields=[("sid", "srv6_sid_t"), ("rewrite", z3.BitVecSort(16)), ("psp", z3.BoolSort()), ("usp", z3.BoolSort()), ("decap", z3.BoolSort()), ("encap", z3.BoolSort()), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_metadata_t", prog_state, fields=[("nexthop", "nexthop_t"), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("egress_metadata_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("lookup_fields_t", prog_state, fields=[("ipv4_dst_addr", z3.BitVecSort(48)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("header_t", prog_state, fields=[("eth_hdr", "ethernet_h"), ], type_params=[])
    )
    prog_state.declare_global(
        ControlDeclaration(P4Parser(
            name="SwitchIngressParser",
            type_params=[],
            params=[
                P4Parameter("none", "pkt", "packet_in", None),
                P4Parameter("out", "hdr", "header_t", None),
                P4Parameter("out", "ig_md", "ingress_metadata_t", None),
                P4Parameter("out", "ig_intr_md", "ingress_intrinsic_metadata_t", None),],
            const_params=[],
            local_decls=[],
            body=ParserTree([
                ParserState(name="start", select="accept",
                components=[                ]),
                ])
))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="SwitchIngressDeparser",
            type_params=[],
            params=[
                P4Parameter("none", "pkt", "packet_out", None),
                P4Parameter("inout", "hdr", "header_t", None),
                P4Parameter("in", "ig_md", "ingress_metadata_t", None),
                P4Parameter("in", "ig_dprsr_md", "ingress_intrinsic_metadata_for_deparser_t", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Parser(
            name="SwitchEgressParser",
            type_params=[],
            params=[
                P4Parameter("none", "pkt", "packet_in", None),
                P4Parameter("out", "hdr", "header_t", None),
                P4Parameter("out", "eg_md", "egress_metadata_t", None),
                P4Parameter("out", "eg_intr_md", "egress_intrinsic_metadata_t", None),],
            const_params=[],
            local_decls=[],
            body=ParserTree([
                ParserState(name="start", select="accept",
                components=[                ]),
                ])
))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="SwitchEgressDeparser",
            type_params=[],
            params=[
                P4Parameter("none", "pkt", "packet_out", None),
                P4Parameter("inout", "hdr", "header_t", None),
                P4Parameter("in", "eg_md", "egress_metadata_t", None),
                P4Parameter("in", "eg_dprsr_md", "egress_intrinsic_metadata_for_deparser_t", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="simple_reset",
            type_params=[],
            params=[
                P4Parameter("in", "dst_addr", z3.BitVecSort(8), None),
                P4Parameter("out", "eth_type", z3.BitVecSort(16), None),],
            const_params=[],
            body=BlockStatement([
                IfStatement(P4eq("dst_addr", z3.BitVecVal(1, 8)), BlockStatement([
                    AssignmentStatement("eth_type", z3.BitVecVal(1, 16)),]
                ), P4Noop()),]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="SwitchIngress",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "header_t", None),
                P4Parameter("inout", "ig_md", "ingress_metadata_t", None),
                P4Parameter("in", "ig_intr_md", "ingress_intrinsic_metadata_t", None),
                P4Parameter("in", "ig_prsr_md", "ingress_intrinsic_metadata_from_parser_t", None),
                P4Parameter("inout", "ig_dprsr_md", "ingress_intrinsic_metadata_for_deparser_t", None),
                P4Parameter("inout", "ig_tm_md", "ingress_intrinsic_metadata_for_tm_t", None),],
            const_params=[],
            body=BlockStatement([
                IfStatement(P4eq(P4Member(P4Member("hdr", "eth_hdr"), "src_addr"), z3.BitVecVal(1, 48)), BlockStatement([
                    AssignmentStatement("undef_0", z3.BitVecVal(1, 8)),]
                ), P4Noop()),
                MethodCallStmt(MethodCallExpr(P4Member("simple_reset_inst_0", "apply"), [], "undef_0", P4Member(P4Member("hdr", "eth_hdr"), "eth_type"), )),]
            ),
            local_decls=[
ValueDeclaration("undef_0", None, z3_type=z3.BitVecSort(8)), 
InstanceDeclaration("simple_reset_inst_0", "simple_reset", ), ]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="SwitchEgress",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "header_t", None),
                P4Parameter("inout", "eg_md", "egress_metadata_t", None),
                P4Parameter("in", "eg_intr_md", "egress_intrinsic_metadata_t", None),
                P4Parameter("in", "eg_intr_from_prsr", "egress_intrinsic_metadata_from_parser_t", None),
                P4Parameter("inout", "eg_intr_md_for_dprsr", "egress_intrinsic_metadata_for_deparser_t", None),
                P4Parameter("inout", "eg_intr_md_for_oport", "egress_intrinsic_metadata_for_output_port_t", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        InstanceDeclaration("pipe", TypeSpecializer("Pipeline", "header_t", "ingress_metadata_t", "header_t", "egress_metadata_t", ), ConstCallExpr("SwitchIngressParser", ), ConstCallExpr("SwitchIngress", ), ConstCallExpr("SwitchIngressDeparser", ), ConstCallExpr("SwitchEgressParser", ), ConstCallExpr("SwitchEgress", ), ConstCallExpr("SwitchEgressDeparser", ), )
    )
    prog_state.declare_global(
        InstanceDeclaration("main", TypeSpecializer("Switch", "header_t", "ingress_metadata_t", "header_t", "egress_metadata_t", None, None, None, None, None, None, None, None, None, None, None, None, ), "pipe", )
    )
    var = prog_state.get_main_function()
    return var if isinstance(var, P4Package) else None
