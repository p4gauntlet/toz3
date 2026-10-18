# Comparing program behavior

The comparator checks whether two P4 programs produce the same observable outputs. We interpret both programs using the same symbolic model and ask the solver for an input on which they disagree. This is the equivalence-checking step described in [§5.2 of the Gauntlet paper](https://www.usenix.org/system/files/osdi20-ruffy.pdf#page=9).

The comparator does not run the compiler or choose which transformations to inspect. It accepts an ordered sequence of programs. This separates the semantic question—whether two programs agree—from the [validation driver's](../validate/README.md) choice of compiler snapshots.

## Comparing functional representations

The [interpreter](../common/README.md) constructs output terms for each programmable block. We flatten these observations into an ordered tuple and compare tuples in a shared Z3 context. Internal statements and control-flow graphs need not correspond. The programs must, however, have compatible observable layouts and symbolic input identities; the comparator does not infer an arbitrary correspondence between unrelated interfaces.

For programs `P` and `Q`, the basic query is:

```text
admissible_inputs ∧ outputs_P ≠ outputs_Q
```

A satisfying assignment gives a counterexample under the interpreter's model. An unsatisfiable query establishes equality under that model. If Z3 returns `unknown`, we report an error rather than treating the comparison as successful.

We simplify the output tuple before comparison. In particular, we normalize some equivalent bit-vector operations so that compiler rewrites do not unnecessarily obscure relationships between packet reads. This reduces the solver's work without requiring the compiler's own normalization passes to run on either input.

## Preserving the input domain

Input constraints are part of the meaning of a summary. For example, an input header union permits at most one valid member. A compiler may flatten that union into separate headers, but the transformation does not make previously illegal input states admissible.

We therefore retain the earlier programs' input constraints as comparison advances through a sequence. Each query uses the accumulated source domain. This prevents a later representation from losing an invariant merely because the type that expressed it has disappeared.

## Comparing defined observations

By default, we check equality of the complete output tuples. With `--allow-undefined`, a mismatch triggers a second check that considers each source output only where it is defined:

```text
admissible_inputs ∧ defined(source_j) ∧ source_j ≠ target_j
```

Definedness follows the selected branch of a conditional expression. An undefined field on one path does not suppress a disagreement on another path, and an unrelated undefined output does not suppress a defined output's disagreement.

This comparison is directional: the source determines which observations must be preserved. It is not symmetric equality of all unspecified values. It also differs from the paper's original taint-substitution procedure; the current implementation computes guarded definedness directly.

## Comparing a sequence

For a sequence `P0, P1, P2`, we compare `P0` with `P1`, then `P1` with `P2`. We interpret programs as needed, keep only adjacent summaries, and stop at the first violation. This limits memory use and avoids interpreting later programs after a counterexample has already been found.

Each pair receives an independent solver query. On a violation, we retain the failing query so diagnostics can print the two summaries and a counterexample assignment. The model is evidence of disagreement in the abstraction; an executable packet test may still be needed to establish the behavior of a particular target.

## Usage

From the P4C build directory:

```sh
./extensions/toz3/p4compare before.p4,after.p4
./extensions/toz3/p4compare --allow-undefined before.p4,after.p4
```

Exit code `0` means the comparisons passed, `20` means a violation was found, and `1` means an error prevented completion. For full compiler runs, use [p4validate](../validate/README.md).

The comparison algorithm and definedness analysis are in [compare.cpp](compare.cpp). Focused regressions are in [tests/gtest/compare.cpp](../tests/gtest/compare.cpp).
