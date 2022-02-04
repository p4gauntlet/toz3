from p4z3 import *



def p4_program(prog_state):
    prog_state.declare_global(
        P4Declaration("max", P4Function("max", return_type=z3.BitVecSort(16), params=[
            P4Parameter("in", "left", z3.BitVecSort(16), None),
            P4Parameter("in", "right", z3.BitVecSort(16), None),],         body=BlockStatement([
            IfStatement(P4gt("left", "right"), P4Return(P4add("left", 1)), P4Noop()),
            P4Return("right"),]
        )        )        )
    )
    prog_state.declare_global(
        ControlDeclaration(P4Control(
            name="c",
            type_params=[],
            params=[
                P4Parameter("out", "b", z3.BitVecSort(16), None),],
            const_params=[],
            body=BlockStatement([
                AssignmentStatement("b", MethodCallExpr("max", [], 10, 12, )),]
            ),
            local_decls=[]
        ))
    )
    prog_state.declare_global(
        ControlDeclaration(P4ControlType("ctr", params=[
            P4Parameter("out", "b", z3.BitVecSort(16), None),], type_params=[]))
    )
    prog_state.declare_global(
        ControlDeclaration(P4Package("top", params=[
            P4Parameter("none", "_c", "ctr", None),],type_params=[]))
    )
    prog_state.declare_global(
        InstanceDeclaration("main", "top", ConstCallExpr("c", ), )
    )
    var = prog_state.get_main_function()
    return var if isinstance(var, P4Package) else None
