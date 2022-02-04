from p4z3 import *



def p4_program(prog_state):
    prog_state.declare_global(
        StructType("S", prog_state, fields=[("x", z3.BitVecSort(32)), ("y", z3.BitVecSort(32)), ], type_params=[])
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="c",
            type_params=[],
            params=[
                P4Parameter("inout", "b", z3.BitVecSort(32), None),],
            const_params=[],
            body=BlockStatement([
                MethodCallStmt(MethodCallExpr("a", [], )),]
            ),
            local_decls=[
P4Declaration("a", P4Action("a", params=[],                 body=BlockStatement([
                    ValueDeclaration("s1", None, z3_type="S"),
                    ValueDeclaration("s2", None, z3_type="S"),
                    AssignmentStatement("s2", [0, 1, ]),
                    AssignmentStatement("s1", "s2"),
                    AssignmentStatement("s2", "s1"),
                    AssignmentStatement("b", P4Member("s2", "x")),]
                )                )), ]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("proto", params=[
            P4Parameter("inout", "_b", z3.BitVecSort(32), None),], type_params=[]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("top", params=[
            P4Parameter("none", "_p", "proto", None),],type_params=[]))
    )
    prog_state.declare_global(
        InstanceDeclaration("main", "top", ConstCallExpr("c", ), )
    )
    var = prog_state.get_main_function()
    return var if isinstance(var, P4Package) else None
