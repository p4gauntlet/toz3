# Validating compiler transformations

The validation driver checks compiler transformations by comparing P4 programs emitted during compilation. We use the compiler to expose its intermediate programs, then interpret those programs independently. This follows the translation-validation approach in [§5 of the Gauntlet paper](https://www.usenix.org/system/files/osdi20-ruffy.pdf#page=7): check a particular compilation rather than prove the compiler correct for all programs.

The driver selects and orders snapshots. The [comparator](../compare/README.md) determines whether their symbolic outputs agree. Keeping these responsibilities separate lets us change how much of the compiler pipeline we inspect without changing the semantic model.

## Checking phase boundaries first

Interpreting every intermediate program can dominate validation time. Many compiler passes preserve the program without introducing a detectable change. By default, we first compare a small set of checkpoints: the first available dump, the frontend result, the midend result, and the final available dump.

```text
compile and discover passes
          |
compare phase-boundary snapshots
          |
       mismatch
          |
compare all available pass snapshots
```

If the checkpoints agree, validation finishes. If they disagree, we generate the full pass sequence and check adjacent programs to locate the first violation. `--all-passes` starts directly with this detailed check.

This is a tradeoff between cost and coverage. A checkpoint check can miss a temporary change that a later pass reverses. A successful checkpoint run therefore does not establish that every intermediate transformation preserved behavior. Use `--all-passes` when that distinction matters.

## Why the detailed result is authoritative

A checkpoint mismatch is a reason to investigate, not necessarily the final result. In particular, undefined-behavior filtering depends on which program is treated as the source. Comparing two distant checkpoints can produce a different result from checking each intervening transformation against its immediate predecessor.

We therefore return the detailed comparison's result after a checkpoint mismatch, even if the detailed comparisons pass. This keeps the reported violation tied to the semantics of an individual transition. The fast stage suppresses counterexample diagnostics; the detailed stage reports them at the failing pair.

## Obtaining the compiler snapshots

We discover the pipeline from the compiler's verbose pass trace rather than hard-code pass numbers. We then invoke its dump mechanism for the selected passes and retain their execution order. Consecutive snapshots with identical contents are removed because comparing them would repeat the same work.

The comparator reparses each retained P4 file. We thus check the programs actually emitted by the compiler, including the effects of serialization, rather than interpreting the compiler's in-memory IR directly. As in the paper, the interpreter does not run the normal frontend transformations before constructing a summary.

The current discovery logic recognizes P4C's `FrontEnd`, `MidEnd`, and `PassManager` trace conventions. It does not automatically support every backend-specific pass manager or every compiler output format. A failed compiler invocation or a missing requested dump is an error; fewer than two retained snapshots yields a skipped check.

## Usage

From the P4C build directory:

```sh
./extensions/toz3/p4validate --compiler-bin ./p4test \
  --dump-dir validated program.p4

./extensions/toz3/p4validate --all-passes --allow-undefined \
  --compiler-bin ./p4test --dump-dir validated program.p4
```

The driver places snapshots under `validated/<program-stem>/checkpoints/` and `validated/<program-stem>/passes/`. These directories distinguish the fast check from the detailed investigation.

Exit code `0` means the selected comparisons passed, `20` means a violation was found, `10` means there was nothing to compare, and `1` means validation failed with an error. Results are relative to the interpreter's [observations and environment models](../common/README.md).

Pass discovery and checkpoint selection are implemented in [passes.cpp](passes.cpp); [main.cpp](main.cpp) connects them to the comparator. [Pass-generation tests](../tests/gtest/passes.cpp) use a simulated compiler to exercise this protocol independently of a particular P4C revision.
