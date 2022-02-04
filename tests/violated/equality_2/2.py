from p4z3 import *



def p4_program(prog_state):
    prog_state.declare_global(
        Enum( "error", ["NoError", "PacketTooShort", "NoMatch", "StackOutOfBounds", "HeaderTooShort", "ParserTimeout", "ParserInvalidArgument", ])
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
        P4Declaration("NoAction", P4Action("NoAction", params=[],         body=BlockStatement([]
        )        ))
    )
    prog_state.declare_global(
        P4Declaration("match_kind", ["exact", "ternary", "lpm", ])
    )
    prog_state.declare_global(
        P4Declaration("match_kind", ["range", "optional", "selector", ])
    )
    prog_state.declare_global(
        ValueDeclaration("__v1model_version", 20180101, z3_type=z3.BitVecSort(32))
    )
    prog_state.declare_global(
        StructType("standard_metadata_t", prog_state, fields=[("ingress_port", z3.BitVecSort(9)), ("egress_spec", z3.BitVecSort(9)), ("egress_port", z3.BitVecSort(9)), ("instance_type", z3.BitVecSort(32)), ("packet_length", z3.BitVecSort(32)), ("enq_timestamp", z3.BitVecSort(32)), ("enq_qdepth", z3.BitVecSort(19)), ("deq_timedelta", z3.BitVecSort(32)), ("deq_qdepth", z3.BitVecSort(19)), ("ingress_global_timestamp", z3.BitVecSort(48)), ("egress_global_timestamp", z3.BitVecSort(48)), ("mcast_grp", z3.BitVecSort(16)), ("egress_rid", z3.BitVecSort(16)), ("checksum_error", z3.BitVecSort(1)), ("parser_error", "error"), ("priority", z3.BitVecSort(3)), ], type_params=[])
    )
    prog_state.declare_global(
        Enum( "CounterType", ["packets", "bytes", "packets_and_bytes", ])
    )
    prog_state.declare_global(
        Enum( "MeterType", ["packets", "bytes", ])
    )
    prog_state.declare_global(
        P4Extern("counter", type_params=[], methods=[P4Declaration("counter", P4Method("counter", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "type", "CounterType", None),])), P4Declaration("count", P4Method("count", type_params=(None, []), params=[
            P4Parameter("in", "index", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        P4Extern("direct_counter", type_params=[], methods=[P4Declaration("direct_counter", P4Method("direct_counter", type_params=(None, []), params=[
            P4Parameter("none", "type", "CounterType", None),])), P4Declaration("count", P4Method("count", type_params=(None, []), params=[])), ])
    )
    prog_state.declare_global(
        P4Extern("meter", type_params=[], methods=[P4Declaration("meter", P4Method("meter", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "type", "MeterType", None),])), P4Declaration("execute_meter", P4Method("execute_meter", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "index", z3.BitVecSort(32), None),
            P4Parameter("out", "result", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("direct_meter", type_params=[
            "T",], methods=[P4Declaration("direct_meter", P4Method("direct_meter", type_params=(None, []), params=[
            P4Parameter("none", "type", "MeterType", None),])), P4Declaration("read", P4Method("read", type_params=(None, []), params=[
            P4Parameter("out", "result", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("register", type_params=[
            "T",], methods=[P4Declaration("register", P4Method("register", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),])), P4Declaration("read", P4Method("read", type_params=(None, []), params=[
            P4Parameter("out", "result", "T", None),
            P4Parameter("in", "index", z3.BitVecSort(32), None),])), P4Declaration("write", P4Method("write", type_params=(None, []), params=[
            P4Parameter("in", "index", z3.BitVecSort(32), None),
            P4Parameter("in", "value", "T", None),])), ])
    )
    prog_state.declare_global(
        P4Extern("action_profile", type_params=[], methods=[P4Declaration("action_profile", P4Method("action_profile", type_params=(None, []), params=[
            P4Parameter("none", "size", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        P4Declaration("random", P4Method("random", type_params=(None, [
            "T",]), params=[
            P4Parameter("out", "result", "T", None),
            P4Parameter("in", "lo", "T", None),
            P4Parameter("in", "hi", "T", None),]))
    )
    prog_state.declare_global(
        P4Declaration("digest", P4Method("digest", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "receiver", z3.BitVecSort(32), None),
            P4Parameter("in", "data", "T", None),]))
    )
    prog_state.declare_global(
        Enum( "HashAlgorithm", ["crc32", "crc32_custom", "crc16", "crc16_custom", "random", "identity", "csum16", "xor16", ])
    )
    prog_state.declare_global(
        P4Declaration("mark_to_drop", P4Method("mark_to_drop", type_params=(None, []), params=[]))
    )
    prog_state.declare_global(
        P4Declaration("mark_to_drop", P4Method("mark_to_drop", type_params=(None, []), params=[
            P4Parameter("inout", "standard_metadata", "standard_metadata_t", None),]))
    )
    prog_state.declare_global(
        P4Declaration("hash", P4Method("hash", type_params=(None, [
            "O",
            "T",
            "D",
            "M",]), params=[
            P4Parameter("out", "result", "O", None),
            P4Parameter("in", "algo", "HashAlgorithm", None),
            P4Parameter("in", "base", "T", None),
            P4Parameter("in", "data", "D", None),
            P4Parameter("in", "max", "M", None),]))
    )
    prog_state.declare_global(
        P4Extern("action_selector", type_params=[], methods=[P4Declaration("action_selector", P4Method("action_selector", type_params=(None, []), params=[
            P4Parameter("none", "algorithm", "HashAlgorithm", None),
            P4Parameter("none", "size", z3.BitVecSort(32), None),
            P4Parameter("none", "outputWidth", z3.BitVecSort(32), None),])), ])
    )
    prog_state.declare_global(
        Enum( "CloneType", ["I2E", "E2E", ])
    )
    prog_state.declare_global(
        P4Extern("Checksum16", type_params=[], methods=[P4Declaration("Checksum16", P4Method("Checksum16", type_params=(None, []), params=[])), P4Declaration("get", P4Method("get", type_params=(z3.BitVecSort(16), [
            "D",]), params=[
            P4Parameter("in", "data", "D", None),])), ])
    )
    prog_state.declare_global(
        P4Declaration("verify_checksum", P4Method("verify_checksum", type_params=(None, [
            "T",
            "O",]), params=[
            P4Parameter("in", "condition", z3.BoolSort(), None),
            P4Parameter("in", "data", "T", None),
            P4Parameter("in", "checksum", "O", None),
            P4Parameter("none", "algo", "HashAlgorithm", None),]))
    )
    prog_state.declare_global(
        P4Declaration("update_checksum", P4Method("update_checksum", type_params=(None, [
            "T",
            "O",]), params=[
            P4Parameter("in", "condition", z3.BoolSort(), None),
            P4Parameter("in", "data", "T", None),
            P4Parameter("inout", "checksum", "O", None),
            P4Parameter("none", "algo", "HashAlgorithm", None),]))
    )
    prog_state.declare_global(
        P4Declaration("verify_checksum_with_payload", P4Method("verify_checksum_with_payload", type_params=(None, [
            "T",
            "O",]), params=[
            P4Parameter("in", "condition", z3.BoolSort(), None),
            P4Parameter("in", "data", "T", None),
            P4Parameter("in", "checksum", "O", None),
            P4Parameter("none", "algo", "HashAlgorithm", None),]))
    )
    prog_state.declare_global(
        P4Declaration("update_checksum_with_payload", P4Method("update_checksum_with_payload", type_params=(None, [
            "T",
            "O",]), params=[
            P4Parameter("in", "condition", z3.BoolSort(), None),
            P4Parameter("in", "data", "T", None),
            P4Parameter("inout", "checksum", "O", None),
            P4Parameter("none", "algo", "HashAlgorithm", None),]))
    )
    prog_state.declare_global(
        P4Declaration("resubmit", P4Method("resubmit", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "data", "T", None),]))
    )
    prog_state.declare_global(
        P4Declaration("recirculate", P4Method("recirculate", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "data", "T", None),]))
    )
    prog_state.declare_global(
        P4Declaration("clone", P4Method("clone", type_params=(None, []), params=[
            P4Parameter("in", "type", "CloneType", None),
            P4Parameter("in", "session", z3.BitVecSort(32), None),]))
    )
    prog_state.declare_global(
        P4Declaration("clone3", P4Method("clone3", type_params=(None, [
            "T",]), params=[
            P4Parameter("in", "type", "CloneType", None),
            P4Parameter("in", "session", z3.BitVecSort(32), None),
            P4Parameter("in", "data", "T", None),]))
    )
    prog_state.declare_global(
        P4Declaration("truncate", P4Method("truncate", type_params=(None, []), params=[
            P4Parameter("in", "length", z3.BitVecSort(32), None),]))
    )
    prog_state.declare_global(
        P4Declaration("assert", P4Method("assert", type_params=(None, []), params=[
            P4Parameter("in", "check", z3.BoolSort(), None),]))
    )
    prog_state.declare_global(
        P4Declaration("assume", P4Method("assume", type_params=(None, []), params=[
            P4Parameter("in", "check", z3.BoolSort(), None),]))
    )
    prog_state.declare_global(
        P4Declaration("log_msg", P4Method("log_msg", type_params=(None, []), params=[
            P4Parameter("none", "msg", z3.StringSort(), None),]))
    )
    prog_state.declare_global(
        P4Declaration("log_msg", P4Method("log_msg", type_params=(None, [
            "T",]), params=[
            P4Parameter("none", "msg", z3.StringSort(), None),
            P4Parameter("in", "data", "T", None),]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ParserType("Parser", params=[
            P4Parameter("none", "b", "packet_in", None),
            P4Parameter("out", "parsedHdr", "H", None),
            P4Parameter("inout", "meta", "M", None),
            P4Parameter("inout", "standard_metadata", "standard_metadata_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("VerifyChecksum", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "meta", "M", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("Ingress", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "meta", "M", None),
            P4Parameter("inout", "standard_metadata", "standard_metadata_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("Egress", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "meta", "M", None),
            P4Parameter("inout", "standard_metadata", "standard_metadata_t", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("ComputeChecksum", params=[
            P4Parameter("inout", "hdr", "H", None),
            P4Parameter("inout", "meta", "M", None),], type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("Deparser", params=[
            P4Parameter("none", "b", "packet_out", None),
            P4Parameter("in", "hdr", "H", None),], type_params=[
            "H",]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("V1Switch", params=[
            P4Parameter("none", "p", TypeSpecializer("Parser", "H", "M", ), None),
            P4Parameter("none", "vr", TypeSpecializer("VerifyChecksum", "H", "M", ), None),
            P4Parameter("none", "ig", TypeSpecializer("Ingress", "H", "M", ), None),
            P4Parameter("none", "eg", TypeSpecializer("Egress", "H", "M", ), None),
            P4Parameter("none", "ck", TypeSpecializer("ComputeChecksum", "H", "M", ), None),
            P4Parameter("none", "dep", TypeSpecializer("Deparser", "H", ), None),],type_params=[
            "H",
            "M",]))
    )
    prog_state.declare_global(
        HeaderType("H", prog_state, fields=[("s", z3.BitVecSort(8)), ("v", z3.BitVecSort(32)), ], type_params=[])
    )
    prog_state.declare_global(
        HeaderType("Same", prog_state, fields=[("same", z3.BitVecSort(8)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("metadata", prog_state, fields=[], type_params=[])
    )
    prog_state.declare_global(
        StructType("headers", prog_state, fields=[("h", "H"), ("a", prog_state.stack("H", 2)), ("same", "Same"), ], type_params=[])
    )
    prog_state.declare_global(
        ControlDeclaration(P4Parser(
            name="p",
            type_params=[],
            params=[
                P4Parameter("none", "b", "packet_in", None),
                P4Parameter("out", "hdr", "headers", None),
                P4Parameter("inout", "meta", "metadata", None),
                P4Parameter("inout", "stdmeta", "standard_metadata_t", None),],
            const_params=[],
            local_decls=[],
            body=ParserTree([
                ParserState(name="start", select="accept",
                components=[
                MethodCallStmt(MethodCallExpr(P4Member("b", "extract"), ["H", ], P4Member("hdr", "h"), z3.BitVecVal(32, 32), )),
                MethodCallStmt(MethodCallExpr(P4Member("b", "extract"), ["H", ], P4Member(P4Member("hdr", "a"), "next"), z3.BitVecVal(32, 32), )),
                MethodCallStmt(MethodCallExpr(P4Member("b", "extract"), ["H", ], P4Member(P4Member("hdr", "a"), "next"), z3.BitVecVal(32, 32), )),                ]),
                ])
))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="ingress",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "headers", None),
                P4Parameter("inout", "meta", "metadata", None),
                P4Parameter("inout", "stdmeta", "standard_metadata_t", None),],
            const_params=[],
            body=BlockStatement([
                MethodCallStmt(MethodCallExpr(P4Member(P4Member("hdr", "same"), "setValid"), [], )),
                AssignmentStatement(P4Member(P4Member("hdr", "same"), "same"), z3.BitVecVal(0, 8)),
                AssignmentStatement(P4Member("stdmeta", "egress_spec"), z3.BitVecVal(0, 9)),
                IfStatement(P4eq(P4Member(P4Member("hdr", "h"), "s"), P4Member(P4Index(P4Member("hdr", "a"), 0), "s")), BlockStatement([
                    AssignmentStatement(P4Member(P4Member("hdr", "same"), "same"), P4bor(P4Member(P4Member("hdr", "same"), "same"), z3.BitVecVal(1, 8))),]
                ), P4Noop()),
                IfStatement(P4eq(P4Member(P4Member("hdr", "h"), "v"), P4Member(P4Index(P4Member("hdr", "a"), 0), "v")), BlockStatement([
                    AssignmentStatement(P4Member(P4Member("hdr", "same"), "same"), P4bor(P4Member(P4Member("hdr", "same"), "same"), z3.BitVecVal(2, 8))),]
                ), P4Noop()),
                IfStatement(P4lor(P4land(P4not(MethodCallExpr(P4Member(P4Member("hdr", "h"), "isValid"), [], )), P4not(MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 0), "isValid"), [], ))), P4land(P4land(P4land(MethodCallExpr(P4Member(P4Member("hdr", "h"), "isValid"), [], ), MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 0), "isValid"), [], )), P4eq(P4Member(P4Member("hdr", "h"), "s"), P4Member(P4Index(P4Member("hdr", "a"), 0), "s"))), P4eq(P4Member(P4Member("hdr", "h"), "v"), P4Member(P4Index(P4Member("hdr", "a"), 0), "v")))), BlockStatement([
                    AssignmentStatement(P4Member(P4Member("hdr", "same"), "same"), P4bor(P4Member(P4Member("hdr", "same"), "same"), z3.BitVecVal(4, 8))),]
                ), P4Noop()),
                AssignmentStatement(P4Index("tmp_0", 0), P4Member("hdr", "h")),
                AssignmentStatement(P4Index("tmp_0", 1), P4Index(P4Member("hdr", "a"), 0)),
                IfStatement(P4land(P4land(z3.BoolVal(True), P4lor(P4land(P4not(MethodCallExpr(P4Member(P4Index("tmp_0", 0), "isValid"), [], )), P4not(MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 0), "isValid"), [], ))), P4land(P4land(P4land(MethodCallExpr(P4Member(P4Index("tmp_0", 0), "isValid"), [], ), MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 0), "isValid"), [], )), P4eq(P4Member(P4Index("tmp_0", 0), "s"), P4Member(P4Index(P4Member("hdr", "a"), 0), "s"))), P4eq(P4Member(P4Index("tmp_0", 0), "v"), P4Member(P4Index(P4Member("hdr", "a"), 0), "v"))))), P4lor(P4land(P4not(MethodCallExpr(P4Member(P4Index("tmp_0", 1), "isValid"), [], )), P4not(MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 1), "isValid"), [], ))), P4land(P4land(P4land(MethodCallExpr(P4Member(P4Index("tmp_0", 1), "isValid"), [], ), MethodCallExpr(P4Member(P4Index(P4Member("hdr", "a"), 1), "isValid"), [], )), P4eq(P4Member(P4Index("tmp_0", 1), "s"), P4Member(P4Index(P4Member("hdr", "a"), 0), "s"))), P4eq(P4Member(P4Index("tmp_0", 1), "v"), P4Member(P4Index(P4Member("hdr", "a"), 1), "v"))))), BlockStatement([
                    AssignmentStatement(P4Member(P4Member("hdr", "same"), "same"), P4bor(P4Member(P4Member("hdr", "same"), "same"), z3.BitVecVal(8, 8))),]
                ), P4Noop()),]
            ),
            local_decls=[
ValueDeclaration("tmp_0", None, z3_type=prog_state.stack("H", 2)), ]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="egress",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "headers", None),
                P4Parameter("inout", "meta", "metadata", None),
                P4Parameter("inout", "stdmeta", "standard_metadata_t", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="vc",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "headers", None),
                P4Parameter("inout", "meta", "metadata", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="uc",
            type_params=[],
            params=[
                P4Parameter("inout", "hdr", "headers", None),
                P4Parameter("inout", "meta", "metadata", None),],
            const_params=[],
            body=BlockStatement([]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="deparser",
            type_params=[],
            params=[
                P4Parameter("none", "packet", "packet_out", None),
                P4Parameter("in", "hdr", "headers", None),],
            const_params=[],
            body=BlockStatement([
                BlockStatement([
                    MethodCallStmt(MethodCallExpr(P4Member("packet", "emit"), ["H", ], P4Member("hdr", "h"), )),
                    MethodCallStmt(MethodCallExpr(P4Member("packet", "emit"), ["H", ], P4Index(P4Member("hdr", "a"), 0), )),
                    MethodCallStmt(MethodCallExpr(P4Member("packet", "emit"), ["H", ], P4Index(P4Member("hdr", "a"), 1), )),
                    MethodCallStmt(MethodCallExpr(P4Member("packet", "emit"), ["Same", ], P4Member("hdr", "same"), )),]
                ),]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        InstanceDeclaration("main", TypeSpecializer("V1Switch", "headers", "metadata", ), ConstCallExpr("p", ), ConstCallExpr("vc", ), ConstCallExpr("ingress", ), ConstCallExpr("egress", ), ConstCallExpr("uc", ), ConstCallExpr("deparser", ), )
    )
    var = prog_state.get_main_function()
    return var if isinstance(var, P4Package) else None