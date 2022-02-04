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
        P4Declaration("match_kind", ["range", "selector", "atcam_partition_index", ])
    )
    prog_state.declare_global(
        HeaderType("ingress_intrinsic_metadata_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_intrinsic_metadata_for_tm_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_intrinsic_metadata_from_parser_t", prog_state, fields=[], type_params=[])
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
        HeaderType("ethernet_h", prog_state, fields=[("dst_addr", z3.BitVecSort(32)), ("src_addr", z3.BitVecSort(32)), ("eth_type", z3.BitVecSort(16)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("ingress_metadata_t", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("egress_metadata_t", prog_state, fields=[], type_params=[])
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
            name="FIB",
            type_params=[],
            params=[
                P4Parameter("in", "dst_addr", z3.BitVecSort(8), None),
                P4Parameter("out", "addr", z3.BitVecSort(16), None),],
            const_params=[],
            body=BlockStatement([
                IfStatement(P4eq("dst_addr", z3.BitVecVal(1, 8)), BlockStatement([
                    AssignmentStatement("addr", z3.BitVecVal(1, 16)),]
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
                IfStatement(P4eq("undef_0", z3.BitVecVal(1, 8)), BlockStatement([
                    AssignmentStatement("undef_0", z3.BitVecVal(1, 8)),]
                ), P4Noop()),
                MethodCallStmt(MethodCallExpr(P4Member("fib_0", "apply"), [], "undef_0", P4Member(P4Member("hdr", "eth_hdr"), "eth_type"), )),]
            ),
            local_decls=[
ValueDeclaration("undef_0", None, z3_type=z3.BitVecSort(8)), 
InstanceDeclaration("fib_0", "FIB", ), ]
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
