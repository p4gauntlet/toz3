This pair reduces a mismatch in P4C `FrontEnd_42_SimplifyDefUse` at commit
`3d9a20e985`. The transformed program removes `headers.first.x = 1` because
it assumes the later assignment always overwrites it.

With a one-byte packet, the first extraction succeeds and the second rejects
with `PacketTooShort`. The original parser returns a valid first header with
`x = 1`; the transformed parser returns the extracted byte instead. For
example, a packet byte of `4` distinguishes them.

P4-16 sections 13.3 and 13.8 specify parser rejection and short-packet
extraction. Rejection does not invalidate previously extracted headers; the
architecture determines what happens to the rejected packet.
See the [P4-16 specification](https://p4lang.github.io/p4-spec/docs/P4-16-working-spec.html).

This is an expected violation test. The independent native regression
`ParserTest.ShortExtractionPreservesEarlierAssignments` checks the interpreter's
result for the one-byte packet against the specified behavior.

## Full-program counterexample

The same defect is present in P4C's `invalid-hdr-warnings4.p4`. Compare dumps
`0042-FrontEnd_41_MoveDeclarations` and `0043-FrontEnd_42_SimplifyDefUse` at the
compiler revision above. Use the four-byte packet `00 00 00 04`:

| Parser output | Before | After |
| --- | --- | --- |
| Acceptance | false | false |
| Error | PacketTooShort | PacketTooShort |
| Cursor, in bits | 32 | 32 |
| `hdr.h1[0].data` | 1 | 4 |

The first extraction succeeds and makes `hdr.h1[0]` valid. The original then
assigns its field to 1. On entering `init`, extracting `hdr.h1[1]` rejects because
no bytes remain. Neither the later reassignment nor invalidation of `hdr.h1[0]`
executes. Removing the earlier assignment therefore changes a valid output
field; this witness does not depend on reading an invalid header.

Both full-program summaries were checked against these expected outputs with
`packet_length("p") = 4` and `packet_read_32("p", 0) = 4`. For each side, asking
whether any listed output differs from its expected value is UNSAT. Asking
whether the field differs between sides is SAT.

Source review points to missing implicit rejection edges in the compiler's
def-use analysis: `ComputeWriteSet` in `frontends/p4/def_use.cpp` propagates
definitions through the parser transition graph. The output-use calculation in
`frontends/p4/simplifyDefUse.cpp` joins accept/reject definitions, but this does
not capture the early exit inside the second extraction. A compiler fix must
retain definitions observable on these implicit rejection paths. Changing
ToZ3 to discard outputs on rejection would hide the defect, especially for
architectures that continue processing rejected packets.

From a P4C build directory, the reduced regression can be reproduced with:

```sh
./extensions/toz3/p4compare --allow-undefined \
  ../extensions/toz3/tests/violated/parser_short_extract_def_use/orig.p4,../extensions/toz3/tests/violated/parser_short_extract_def_use/0.p4
```

The expected exit code is 20. The compiler-side defect remains unfixed here.
