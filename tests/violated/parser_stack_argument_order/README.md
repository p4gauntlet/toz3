This pair reduces a mismatch in P4C `FrontEnd_37_SideEffectOrdering` at
commit `3d9a20e985`, also encountered while validating `issue2090.p4`.
The pass moves the variable-size argument's lookahead before evaluation of
`headers.stack.next.b`.

For a one-byte packet, the first extraction fills the stack. The original
program rejects when evaluating the second extraction's first argument, with
`StackOutOfBounds`. The transformed program first executes the lookahead at
bit offset 8 and rejects with `PacketTooShort`. The cursor stays at 8 in both.

P4-16 section 6.8 requires evaluation of arguments from left to right, including
saving out-argument l-values. Section 8.20 specifies rejection on evaluation of
an out-of-bounds `stack.next` expression.
See the [P4-16 specification](https://p4lang.github.io/p4-spec/docs/P4-16-working-spec.html).

This test expects a violation, rather than equivalence. The native stack
extraction regression independently checks `StackOutOfBounds` after successful
extractions have filled the stack, including when no packet bytes remain.

## Full-program counterexamples

Both `issue2090.p4` and `spec-ex19.p4` exhibit this defect at the compiler revision
above. Their last distinct snapshot before SideEffectOrdering is
`0032-FrontEnd_31_RemoveAllUnusedDeclarations`; the changed snapshot is
`0038-FrontEnd_37_SideEffectOrdering`.

Use this eleven-byte packet for either program:

```text
01 01 01 01 01 01 01 01 01 01 05
```

| Parser output | Before | After |
| --- | --- | --- |
| Acceptance | false | false |
| Error | StackOutOfBounds | PacketTooShort |
| Cursor, in bits | 80 | 80 |

Each of the first ten bytes selects the `nop` state, which extracts one byte into
`vec.next.nop`. The ten-element stack is then full. At cursor 80 the final byte
selects `sack`, using an eight-bit lookahead that succeeds without advancing.

The original call evaluates `vec.next.sack` first. Its out-of-bounds `.next`
rejects before the size argument is evaluated. The transformed program instead
first assigns `b.lookahead<Tcp_option_sack_top>()` to a temporary. This lookahead
requires 24 bits in `issue2090` and 16 bits in `spec-ex19`; only eight remain.
It rejects before the extraction call. Neither version executes a variable-width
extraction on this packet, so the interpreter's incomplete runtime `varbit`
length model does not affect this counterexample.

For both full-program pairs, fix `packet_length("p") = 11`,
`packet_read_8("p", 8*i) = 1` for `0 <= i < 10`, and
`packet_read_8("p", 80) = 5`. The error comparison is SAT. Independently asking
whether either side differs from its listed acceptance, error, or cursor is
UNSAT under these inputs.

The unconstrained models can contain inconsistent overlapping reads because
ToZ3 currently uses separate functions for different packet widths. As an
additional diagnostic, fixed-width reads of 8, 16, 24, 32, and 40 bits were
rewritten as concatenations of a common packet-byte function. Both error
comparisons remained SAT. The concrete packet above needs only eight-bit reads
and avoids this abstraction issue entirely.

Source review identifies an ordering gap in `frontends/p4/sideEffects.cpp`:
`DoSimplifyExpressions::preorder(MethodCallExpression)` hoists the nested
lookahead while leaving the first argument in the eventual extraction call.
The `SideEffects` inspector in `sideEffects.h` recognizes method/constructor
calls, but not a member access such as `.next` that can reject. A compiler fix
must preserve evaluation of the saved out-argument l-value, including its bounds
check, before the hoisted lookahead. Merely introducing an out-header temporary
and delaying the l-value evaluation until copy-out would still be incorrect.

From a P4C build directory, the reduced regression can be reproduced with:

```sh
./extensions/toz3/p4compare --allow-undefined \
  ../extensions/toz3/tests/violated/parser_stack_argument_order/orig.p4,../extensions/toz3/tests/violated/parser_stack_argument_order/0.p4
```

The expected exit code is 20. These are isolated parser-summary violations:
although both versions reject, an architecture can observe the recorded error.
The compiler-side defect remains unfixed here.
