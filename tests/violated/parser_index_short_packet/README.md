# Explicit stack indexing and parser unrolling

At P4C `54eb2f5607`, `ParsersUnroll` replaces an extraction through an out-of-range
runtime stack index with immediate `StackOutOfBounds` rejection. This differs
from `.next`/`.last`: the specification's
[stack indexing rules](https://github.com/p4lang/p4-spec/blob/main/p4-16/spec/P4-16-spec.adoc#sec-expr-hs)
make ordinary out-of-range runtime indexing undefined, without specifying that
parser error. The extraction still performs its packet-length check.

For the one-byte packet `00`, the source consumes one byte, then the second
extraction rejects with `PacketTooShort`. The lowered program instead rejects
with `StackOutOfBounds`. Both cursors are 8 bits. No undefined header field is
read before the source rejection, and the 8-round fallback is never reached.

Both comparing `orig.p4,0.p4` and validating `orig.p4` with `--all-passes` report
a violation (exit 20), including with `--allow-undefined`.

The full sample `parser-unroll-test1.p4` has the same mismatch for this 20-byte
packet: twelve zero bytes, `12 34`, then six zero bytes. Its three-element stack
fills and the fourth extraction fails. The cursor is 160 bits on both sides;
the source error is `PacketTooShort`, while the unrolled error is
`StackOutOfBounds`.
