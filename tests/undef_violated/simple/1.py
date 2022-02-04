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
        HeaderType("H", prog_state, fields=[("a", z3.BitVecSort(8)), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("Headers", prog_state, fields=[("h", "H"), ], type_params=[])
    )
    prog_state.declare_global(
        StructType("Meta", prog_state, fields=[], type_params=[])
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
                MethodCallStmt(MethodCallExpr(P4Member("pkt", "extract"), [], P4Member("hdr", "h"), )),                ]),
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
                MethodCallStmt(MethodCallExpr(P4Member(P4Member("h", "h"), "setValid"), [], )),
                ValueDeclaration("tmp", None, z3_type=z3.BitVecSort(8)),
                AssignmentStatement(P4Member(P4Member("h", "h"), "a"), P4add("tmp", 1)),]
            ),
            local_decls=[]
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
