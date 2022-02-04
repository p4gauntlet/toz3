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
        HeaderType("ethernet_t", prog_state, fields=[("dst_addr", z3.BitVecSort(48)), ("src_addr", z3.BitVecSort(48)), ("eth_type", z3.BitVecSort(16)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("Headers", prog_state, fields=[("eth_hdr", "ethernet_t"), ], type_params=[])
    )
    prog_state.declare_global(
        ControlDeclaration(P4Parser(
            name="p",
            type_params=[],
            params=[
                P4Parameter("none", "pkt", "packet_in", None),
                P4Parameter("out", "hdr", "Headers", None),],
            const_params=[],
            local_decls=[],
            body=ParserTree([
                ParserState(name="start", select="parse_hdrs",
                components=[                ]),
                ParserState(name="parse_hdrs", select="accept",
                components=[
                MethodCallStmt(MethodCallExpr(P4Member("pkt", "extract"), [], P4Member("hdr", "eth_hdr"), )),                ]),
                ])
))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="ingress",
            type_params=[],
            params=[
                P4Parameter("inout", "h", "Headers", None),],
            const_params=[],
            body=BlockStatement([
                SwitchStatement(P4Member(MethodCallExpr(P4Member("simple_table", "apply"), [], ), "action_run"),cases=[("simple_action", BlockStatement([
                    AssignmentStatement(P4Member(P4Member("h", "eth_hdr"), "eth_type"), z3.BitVecVal(1, 16)),
                    P4Exit(),]
                )), ("NoAction", BlockStatement([
                    AssignmentStatement(P4Member(P4Member("h", "eth_hdr"), "eth_type"), z3.BitVecVal(2, 16)),
                    P4Exit(),]
                )), (DefaultExpression(), BlockStatement([
                    AssignmentStatement(P4Member(P4Member("h", "eth_hdr"), "eth_type"), z3.BitVecVal(3, 16)),
                    P4Exit(),]
                )), ]),
                AssignmentStatement(P4Member(P4Member("h", "eth_hdr"), "eth_type"), z3.BitVecVal(4, 16)),
                P4Exit(),]
            ),
            local_decls=[
P4Declaration("simple_action", P4Action("simple_action", params=[],                 body=BlockStatement([
                    AssignmentStatement(P4Member(P4Member("h", "eth_hdr"), "src_addr"), 1),]
                )                )), 
P4Declaration("NoAction_dummy", P4Action("NoAction_dummy", params=[],                 body=BlockStatement([]
                )                )), 
P4Declaration("simple_table", P4Table("simple_table", key=[(z3.BitVecVal(1, 48), "exact"), ], actions=[MethodCallExpr("simple_action", [], ), MethodCallExpr("NoAction", [], ), MethodCallExpr("NoAction_dummy", [], ), ], immutable=False)), ]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ParserType("Parser", params=[
            P4Parameter("none", "b", "packet_in", None),
            P4Parameter("out", "hdr", "Headers", None),], type_params=[]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("Ingress", params=[
            P4Parameter("inout", "hdr", "Headers", None),], type_params=[]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("top", params=[
            P4Parameter("none", "p", "Parser", None),
            P4Parameter("none", "ig", "Ingress", None),],type_params=[]))
    )
    prog_state.declare_global(
        InstanceDeclaration("main", "top", ConstCallExpr("p", ), ConstCallExpr("ingress", ), )
    )
    var = prog_state.get_main_function()
    return var if isinstance(var, P4Package) else None
