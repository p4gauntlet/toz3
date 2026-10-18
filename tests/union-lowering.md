# Header-union lowering regressions

The interpreter uses the P4-16 v1.2.5 specification, particularly
[header-union operations](https://github.com/p4lang/p4-spec/blob/main/p4-16/spec/P4-16-spec.adoc#sec-expr-hu),
[header operations](https://github.com/p4lang/p4-spec/blob/main/p4-16/spec/P4-16-spec.adoc#sec-ops-on-hdrs),
and [extraction](https://github.com/p4lang/p4-spec/blob/main/p4-16/spec/P4-16-spec.adoc#sec-packet-extract-one).
The following reduced programs produce interpreter-reported mismatches at P4C `54eb2f5607`.
Each `orig.p4,0.p4` comparison returns **20**, even with `--allow-undefined`.
Compiling and validating each `orig.p4` with `--all-passes` also finds a violation.

**Independent packet tests confirm the field-write and extraction lowering bugs.**
The BMv2/STF reproducers in [P4C #5862](https://github.com/p4lang/p4c/issues/5862) and [#5863](https://github.com/p4lang/p4c/issues/5863) pass before
`FlattenHeaderUnion` and fail afterward, without using ToZ3. They also expose an
important qualification: BMv2 disagrees with the literal `setInvalid()` wording
even on the source, and its union-copy test passes before and after lowering.
The specification discussion [confirms whole-union invalidation](https://github.com/p4lang/p4-spec/issues/1221#issuecomment-1384474748). The invalidation, copy, and retained-type reports are tracked in [#4448](https://github.com/p4lang/p4c/issues/4448#issuecomment-6222080038), [#3842](https://github.com/p4lang/p4c/issues/3842#issuecomment-6222080326), and [#5864](https://github.com/p4lang/p4c/issues/5864); they are not independently confirmed packet-level lowering bugs. The table below records ToZ3's specification interpretation.

## Defined validity counterexamples

These witnesses compare validity bits, so unspecified field contents do not
explain their differences.

| Fixture | Input | Spec/source result | Lowered result |
| --- | --- | --- | --- |
| [union_field_write](violated/union_field_write) | `u.a` valid, `u.b` invalid | Writing `u.b.x` does not validate `b`; `a` stays valid | `b` becomes valid and `a` invalid |
| [union_invalidate](violated/union_invalidate) | `u.a` valid, `u.b` invalid | `u.b.setInvalid()` clears the union | Only flattened `b` is invalidated; `a` stays valid |
| [union_copy](violated/union_copy) | Source `a` valid, source `b` invalid | Whole-union copy leaves destination `a` valid | Copying `b` last invalidates destination `a` |
| [union_extract](violated/union_extract) | `u.b` valid, one-byte packet (e.g. `07`) | Successful extraction into `a` invalidates `b` | Both flattened headers remain valid |
| [union_retained_type](violated/union_retained_type) | `u.a` valid, `u.b` invalid | Writing valid `a.x` leaves `a` valid | Added `u.b.setInvalid()` clears the still-union-typed value |

The specification explicitly says that `u.hi.setInvalid()` clears whichever
member is valid, and that assigning an invalid header to a union member has the
same effect. Whole-union assignment instead copies the entire state. Consequently,
an unconditional sequence of member assignments is not equivalent to a union copy.

## Interpreter-reported passes and affected samples

`MidEnd_18_CopyStructures` expands the union copy into member assignments. This
is the first failing transition for `pna-dpdk-header-union-stack` and
`pna-dpdk-header-union-stack1`.

`MidEnd_48_FlattenHeaderUnion` exposes the other four reduced witnesses:

- `HandleValidityHeaderUnion` inserts `setValid()` when writing a constant field.
  This changes invalid-header writes in `invalid-hdr-warnings5/6`, their PNA
  variants, and `pna-dpdk-header-union-stack2`.
- Ordinary `setInvalid()` calls are not expanded to clear every flattened
  union member, as demonstrated by `union4-bmv2`.
- Extraction into an `inout` union member does not invalidate the other flattened
  members. `header-union-field-name-collision-bmv2` exposes this after its
  `MidEnd_49_FlattenHeaderUnion` (the extra pass is due to loop unrolling).
- Some union parameters and stacks remain union-typed while the pass inserts
  sibling invalidations. This affects `invalid-union` and `issue561-4/5/6-bmv2`.

Only these affected samples are expected failures. There is no pass-wide
exclusion: other union samples, including `union-bmv2`, `union-valid-bmv2`, and
`psa-dpdk-header-union-typedef`, are checked normally.

## Reproduction

From the P4C build directory, substitute any fixture name from the table:

```sh
./p4compare --allow-undefined \
  ../extensions/toz3/tests/violated/union_extract/orig.p4,../extensions/toz3/tests/violated/union_extract/0.p4

./p4validate --all-passes --allow-undefined --compiler-bin ./p4test \
  --dump-dir union-dumps ../extensions/toz3/tests/violated/union_extract/orig.p4
```

Both commands should return 20. Interpreter unit tests verify the implemented
validity rules, copy isolation, path merging, and preservation of the prior union
member when extraction fails on a short packet.
